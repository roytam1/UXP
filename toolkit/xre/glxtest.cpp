/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//////////////////////////////////////////////////////////////////////////////
//
// Explanation: See bug 639842. Safely getting GL driver info on X11 is hard, because the only way to do
// that is to create a GL context and call glGetString(), but with bad drivers,
// just creating a GL context may crash.
//
// This file implements the idea to do that in a separate process.
//
// The only non-static function here is fire_glxtest_process(). It creates a pipe, publishes its 'read' end as the
// mozilla::widget::glxtest_pipe global variable, forks, and runs that X11 GL probe in the child process,
// which runs the glxtest() static function. This creates a X connection, a GL context, calls glGetString, and writes that
// to the 'write' end of the pipe.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <dlfcn.h>
#include <strings.h>
#include "nscore.h"
#include <fcntl.h>
#include "stdint.h"

#if MOZ_WIDGET_GTK == 2
#include <glib.h>
#endif

#ifdef __SUNPRO_CC
#include <stdio.h>
#endif

#include "X11/Xlib.h"
#include "X11/Xutil.h"

#include "EGL/egl.h"

#include "mozilla/Unused.h"

#if defined(__OpenBSD__) || defined(__NetBSD__)
# define LIBGL_FILENAME "libGL.so"
# define LIBGL_FILENAME_FALLBACK "libGL.so.1"
#else
# define LIBGL_FILENAME "libGL.so.1"
# define LIBGL_FILENAME_FALLBACK "libGL.so"
#endif

// stuff from glx.h
typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
#define GLX_RGBA        4
#define GLX_RED_SIZE    8
#define GLX_GREEN_SIZE  9
#define GLX_BLUE_SIZE   10

// stuff from gl.h
typedef uint8_t GLubyte;
typedef uint32_t GLenum;
#define GL_VENDOR       0x1F00
#define GL_RENDERER     0x1F01
#define GL_VERSION      0x1F02

namespace mozilla {
namespace widget {
// the read end of the pipe, which will be used by GfxInfo
extern int glxtest_pipe;
// the PID of the glxtest process, to pass to waitpid()
extern pid_t glxtest_pid;
}
}

// the write end of the pipe, which we're going to write to
static int write_end_of_the_pipe = -1;
static std::string sAutoFallbackMessage;

#if MOZ_WIDGET_GTK == 2
static int gtk_write_end_of_the_pipe = -1;
int gtk_read_end_of_the_pipe = -1;
#endif

// C++ standard collides with C standard in that it doesn't allow casting void* to function pointer types.
// So the work-around is to convert first to size_t.
// http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
template<typename func_ptr_type>
static func_ptr_type cast(void *ptr)
{
  return reinterpret_cast<func_ptr_type>(
           reinterpret_cast<size_t>(ptr)
         );
}

static void
write_to_pipe(const char* str, size_t len)
{
  if (str && len) {
    mozilla::Unused << write(write_end_of_the_pipe, str, len);
  }
}

static void
write_to_pipe(const std::string& str)
{
  write_to_pipe(str.c_str(), str.size());
}

static void fatal_error(const char *str)
{
  if (!sAutoFallbackMessage.empty()) {
    write_to_pipe(sAutoFallbackMessage);
  }
  write_to_pipe(str, strlen(str));
  write_to_pipe("\n", 1);
  _exit(EXIT_FAILURE);
}

enum class ProbeMode {
  Auto,
  EGL,
  GLX
};

static ProbeMode
get_probe_mode()
{
  const char* envValue = getenv("MOZ_X11_GL_PROVIDER");
  if (!envValue || !*envValue) {
    return ProbeMode::Auto;
  }

  if (!strcasecmp(envValue, "egl")) {
    return ProbeMode::EGL;
  }

  if (!strcasecmp(envValue, "glx")) {
    return ProbeMode::GLX;
  }

  return ProbeMode::Auto;
}

static bool
load_library(const char* aPrimary,
             const char* aSecondary,
             void** aOutLibrary,
             std::string* aError)
{
  *aOutLibrary = dlopen(aPrimary, RTLD_LAZY);
  if (!*aOutLibrary && aSecondary) {
    *aOutLibrary = dlopen(aSecondary, RTLD_LAZY);
  }

  if (*aOutLibrary) {
    return true;
  }

  *aError = std::string("Unable to load ") + aPrimary;
  if (aSecondary) {
    *aError += " or ";
    *aError += aSecondary;
  }
  return false;
}

static int
x_error_handler(Display *, XErrorEvent *ev)
{
  enum { bufsize = 1024 };
  char buf[bufsize];
  int length = snprintf(buf, bufsize,
                        "X error occurred in X11 GL probe, error_code=%d, request_code=%d, minor_code=%d\n",
                        ev->error_code,
                        ev->request_code,
                        ev->minor_code);
  if (!sAutoFallbackMessage.empty()) {
    write_to_pipe(sAutoFallbackMessage);
  }
  write_to_pipe(buf, length);
  _exit(EXIT_FAILURE);
  return 0;
}

static bool
try_egltest(std::string* aError)
{
  static const EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 0,
    EGL_NONE
  };
  static const EGLint surfaceAttribs[] = {
    EGL_WIDTH, 16,
    EGL_HEIGHT, 16,
    EGL_NONE
  };
  static const EGLint contextAttribs[] = {
    EGL_NONE
  };

  void* libegl = nullptr;
  if (!load_library("libEGL.so", "libEGL.so.1", &libegl, aError)) {
    return false;
  }

  void* libgl = nullptr;
  if (!load_library(LIBGL_FILENAME, LIBGL_FILENAME_FALLBACK, &libgl, aError)) {
    dlclose(libegl);
    return false;
  }

  typedef EGLDisplay (* PFNEGLGETDISPLAY)(EGLNativeDisplayType);
  typedef EGLBoolean (* PFNEGLINITIALIZE)(EGLDisplay, EGLint*, EGLint*);
  typedef EGLBoolean (* PFNEGLBINDAPI)(EGLenum);
  typedef EGLBoolean (* PFNEGLCHOOSECONFIG)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
  typedef EGLContext (* PFNEGLCREATECONTEXT)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
  typedef EGLSurface (* PFNEGLCREATEPBUFFERSURFACE)(EGLDisplay, EGLConfig, const EGLint*);
  typedef EGLBoolean (* PFNEGLMAKECURRENT)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
  typedef EGLBoolean (* PFNEGLDESTROYCONTEXT)(EGLDisplay, EGLContext);
  typedef EGLBoolean (* PFNEGLDESTROYSURFACE)(EGLDisplay, EGLSurface);
  typedef EGLBoolean (* PFNEGLTERMINATE)(EGLDisplay);
  typedef EGLint (* PFNEGLGETERROR)(void);
  typedef const GLubyte* (* PFNGLGETSTRING)(GLenum);

  PFNEGLGETDISPLAY eglGetDisplay = cast<PFNEGLGETDISPLAY>(dlsym(libegl, "eglGetDisplay"));
  PFNEGLINITIALIZE eglInitialize = cast<PFNEGLINITIALIZE>(dlsym(libegl, "eglInitialize"));
  PFNEGLBINDAPI eglBindAPI = cast<PFNEGLBINDAPI>(dlsym(libegl, "eglBindAPI"));
  PFNEGLCHOOSECONFIG eglChooseConfig = cast<PFNEGLCHOOSECONFIG>(dlsym(libegl, "eglChooseConfig"));
  PFNEGLCREATECONTEXT eglCreateContext = cast<PFNEGLCREATECONTEXT>(dlsym(libegl, "eglCreateContext"));
  PFNEGLCREATEPBUFFERSURFACE eglCreatePbufferSurface =
    cast<PFNEGLCREATEPBUFFERSURFACE>(dlsym(libegl, "eglCreatePbufferSurface"));
  PFNEGLMAKECURRENT eglMakeCurrent = cast<PFNEGLMAKECURRENT>(dlsym(libegl, "eglMakeCurrent"));
  PFNEGLDESTROYCONTEXT eglDestroyContext = cast<PFNEGLDESTROYCONTEXT>(dlsym(libegl, "eglDestroyContext"));
  PFNEGLDESTROYSURFACE eglDestroySurface = cast<PFNEGLDESTROYSURFACE>(dlsym(libegl, "eglDestroySurface"));
  PFNEGLTERMINATE eglTerminate = cast<PFNEGLTERMINATE>(dlsym(libegl, "eglTerminate"));
  PFNEGLGETERROR eglGetError = cast<PFNEGLGETERROR>(dlsym(libegl, "eglGetError"));
  PFNGLGETSTRING glGetString = cast<PFNGLGETSTRING>(dlsym(libgl, "glGetString"));

  if (!eglGetDisplay ||
      !eglInitialize ||
      !eglBindAPI ||
      !eglChooseConfig ||
      !eglCreateContext ||
      !eglCreatePbufferSurface ||
      !eglMakeCurrent ||
      !eglDestroyContext ||
      !eglDestroySurface ||
      !eglTerminate ||
      !eglGetError ||
      !glGetString)
  {
    *aError = "EGL probe couldn't find required EGL/OpenGL symbols";
    dlclose(libgl);
    dlclose(libegl);
    return false;
  }

  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    *aError = "Unable to open a connection to the X server";
    dlclose(libgl);
    dlclose(libegl);
    return false;
  }

  EGLDisplay eglDisplay = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  EGLConfig config = nullptr;
  EGLint major = 0;
  EGLint minor = 0;
  EGLint configCount = 0;
  const GLubyte* vendorString = nullptr;
  const GLubyte* rendererString = nullptr;
  const GLubyte* versionString = nullptr;
  enum { bufsize = 1024 };
  char buf[bufsize];
  int length = 0;

  eglDisplay = eglGetDisplay((EGLNativeDisplayType)dpy);
  if (eglDisplay == EGL_NO_DISPLAY) {
    *aError = "EGL probe could not get an EGL display";
    goto egltest_error;
  }

  if (!eglInitialize(eglDisplay, &major, &minor)) {
    *aError = "EGL probe could not initialize EGL";
    goto egltest_error;
  }

  if (!eglBindAPI(EGL_OPENGL_API)) {
    *aError = "EGL probe could not bind the OpenGL API";
    goto egltest_error;
  }

  if (!eglChooseConfig(eglDisplay, configAttribs, &config, 1, &configCount) ||
      configCount < 1)
  {
    *aError = "EGL probe could not find a suitable EGLConfig";
    goto egltest_error;
  }

  surface = eglCreatePbufferSurface(eglDisplay, config, surfaceAttribs);
  if (surface == EGL_NO_SURFACE) {
    *aError = "EGL probe could not create a pbuffer surface";
    goto egltest_error;
  }

  context = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
  if (context == EGL_NO_CONTEXT) {
    eglDestroySurface(eglDisplay, surface);
    *aError = "EGL probe could not create an EGL context";
    goto egltest_error;
  }

  if (!eglMakeCurrent(eglDisplay, surface, surface, context)) {
    eglDestroyContext(eglDisplay, context);
    eglDestroySurface(eglDisplay, surface);
    *aError = "EGL probe could not make the EGL context current";
    goto egltest_error;
  }

  vendorString = glGetString(GL_VENDOR);
  rendererString = glGetString(GL_RENDERER);
  versionString = glGetString(GL_VERSION);

  if (!vendorString || !rendererString || !versionString) {
    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDisplay, context);
    eglDestroySurface(eglDisplay, surface);
    *aError = "EGL probe glGetString returned null";
    goto egltest_error;
  }

  length = snprintf(buf, bufsize,
                    "PROVIDER\nEGL\nVENDOR\n%s\nRENDERER\n%s\nVERSION\n%s\nTFP\nFALSE\n",
                    vendorString,
                    rendererString,
                    versionString);
  if (length >= bufsize) {
    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDisplay, context);
    eglDestroySurface(eglDisplay, surface);
    *aError = "EGL probe GL strings length too large for buffer size";
    goto egltest_error;
  }

  eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(eglDisplay, context);
  eglDestroySurface(eglDisplay, surface);
  eglTerminate(eglDisplay);

#ifdef NS_FREE_PERMANENT_DATA
  XCloseDisplay(dpy);
#else
  XSync(dpy, False);
#endif

  dlclose(libgl);
  dlclose(libegl);

  write_to_pipe(buf, length);
  return true;

egltest_error:
  if (!aError->empty() && eglGetError) {
    char eglErrorBuf[128];
    int length = snprintf(eglErrorBuf, sizeof(eglErrorBuf),
                          " (EGL error 0x%04x)", eglGetError());
    if (length > 0 && size_t(length) < sizeof(eglErrorBuf)) {
      aError->append(eglErrorBuf);
    }
  }
#ifdef NS_FREE_PERMANENT_DATA
  XCloseDisplay(dpy);
#else
  XSync(dpy, False);
#endif
  dlclose(libgl);
  dlclose(libegl);
  return false;
}


// glxtest is declared inside extern "C" so that the name is not mangled.
// The name is used in build/valgrind/x86_64-redhat-linux-gnu.sup to suppress
// memory leak errors because we run it inside a short lived fork and we don't
// care about leaking memory
extern "C" {

void glxtest()
{
  // we want to redirect to /dev/null stdout, stderr, and while we're at it,
  // any PR logging file descriptors. To that effect, we redirect all positive
  // file descriptors up to what open() returns here. In particular, 1 is stdout and 2 is stderr.
  int fd = open("/dev/null", O_WRONLY);
  for (int i = 1; i < fd; i++)
    dup2(fd, i);
  close(fd);

#if MOZ_WIDGET_GTK == 2
  // On Gtk+2 builds, try to get the Gtk+3 version if it's installed, and
  // use that in nsSystemInfo for secondaryLibrary. Better safe than sorry,
  // we want to load the Gtk+3 library in a subprocess, and since we already
  // have such a subprocess for the GLX test, we piggy back on it.
  void *gtk3 = dlopen("libgtk-3.so.0", RTLD_LOCAL | RTLD_LAZY);
  if (gtk3) {
    auto gtk_get_major_version = reinterpret_cast<guint (*)(void)>(
      dlsym(gtk3, "gtk_get_major_version"));
    auto gtk_get_minor_version = reinterpret_cast<guint (*)(void)>(
      dlsym(gtk3, "gtk_get_minor_version"));
    auto gtk_get_micro_version = reinterpret_cast<guint (*)(void)>(
      dlsym(gtk3, "gtk_get_micro_version"));

    if (gtk_get_major_version && gtk_get_minor_version &&
        gtk_get_micro_version) {
      // 64 bytes is going to be well enough for "GTK " followed by 3 integers
      // separated with dots.
      char gtkver[64];
      int len = snprintf(gtkver, sizeof(gtkver), "GTK %u.%u.%u",
                         gtk_get_major_version(), gtk_get_minor_version(),
                         gtk_get_micro_version());
      if (len > 0 && size_t(len) < sizeof(gtkver)) {
        mozilla::Unused << write(gtk_write_end_of_the_pipe, gtkver, len);
      }
    }
  }
#endif


  if (getenv("MOZ_AVOID_OPENGL_ALTOGETHER"))
    fatal_error("The MOZ_AVOID_OPENGL_ALTOGETHER environment variable is defined");

  ProbeMode probeMode = get_probe_mode();
  if (probeMode != ProbeMode::GLX) {
    std::string eglError;
    if (try_egltest(&eglError)) {
      return;
    }

    if (probeMode == ProbeMode::EGL) {
      fatal_error(eglError.c_str());
    }

    sAutoFallbackMessage = "EGL probe failed, falling back to GLX: " + eglError + "\n";
  }

  ///// Open libGL and load needed symbols /////
  void* libgl = nullptr;
  std::string libglError;
  if (!load_library(LIBGL_FILENAME, LIBGL_FILENAME_FALLBACK, &libgl, &libglError))
    fatal_error(libglError.c_str());
  
  typedef void* (* PFNGLXGETPROCADDRESS) (const char *);
  PFNGLXGETPROCADDRESS glXGetProcAddress = cast<PFNGLXGETPROCADDRESS>(dlsym(libgl, "glXGetProcAddress"));
  
  if (!glXGetProcAddress)
    fatal_error("Unable to find glXGetProcAddress in " LIBGL_FILENAME);

  typedef GLXFBConfig* (* PFNGLXQUERYEXTENSION) (Display *, int *, int *);
  PFNGLXQUERYEXTENSION glXQueryExtension = cast<PFNGLXQUERYEXTENSION>(glXGetProcAddress("glXQueryExtension"));

  typedef GLXFBConfig* (* PFNGLXQUERYVERSION) (Display *, int *, int *);
  PFNGLXQUERYVERSION glXQueryVersion = cast<PFNGLXQUERYVERSION>(dlsym(libgl, "glXQueryVersion"));

  typedef XVisualInfo* (* PFNGLXCHOOSEVISUAL) (Display *, int, int *);
  PFNGLXCHOOSEVISUAL glXChooseVisual = cast<PFNGLXCHOOSEVISUAL>(glXGetProcAddress("glXChooseVisual"));

  typedef GLXContext (* PFNGLXCREATECONTEXT) (Display *, XVisualInfo *, GLXContext, Bool);
  PFNGLXCREATECONTEXT glXCreateContext = cast<PFNGLXCREATECONTEXT>(glXGetProcAddress("glXCreateContext"));

  typedef Bool (* PFNGLXMAKECURRENT) (Display*, GLXDrawable, GLXContext);
  PFNGLXMAKECURRENT glXMakeCurrent = cast<PFNGLXMAKECURRENT>(glXGetProcAddress("glXMakeCurrent"));

  typedef void (* PFNGLXDESTROYCONTEXT) (Display*, GLXContext);
  PFNGLXDESTROYCONTEXT glXDestroyContext = cast<PFNGLXDESTROYCONTEXT>(glXGetProcAddress("glXDestroyContext"));

  typedef GLubyte* (* PFNGLGETSTRING) (GLenum);
  PFNGLGETSTRING glGetString = cast<PFNGLGETSTRING>(glXGetProcAddress("glGetString"));

  if (!glXQueryExtension ||
      !glXQueryVersion ||
      !glXChooseVisual ||
      !glXCreateContext ||
      !glXMakeCurrent ||
      !glXDestroyContext ||
      !glGetString)
  {
    fatal_error("glXGetProcAddress couldn't find required functions");
  }
  ///// Open a connection to the X server /////
  Display *dpy = XOpenDisplay(nullptr);
  if (!dpy)
    fatal_error("Unable to open a connection to the X server");
  
  ///// Check that the GLX extension is present /////
  if (!glXQueryExtension(dpy, nullptr, nullptr))
    fatal_error("GLX extension missing");

  XSetErrorHandler(x_error_handler);

  ///// Get a visual /////
   int attribs[] = {
      GLX_RGBA,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      None };
  XVisualInfo *vInfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
  if (!vInfo)
    fatal_error("No visuals found");

  // using a X11 Window instead of a GLXPixmap does not crash
  // fglrx in indirect rendering. bug 680644
  Window window;
  XSetWindowAttributes swa;
  swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vInfo->screen),
                                 vInfo->visual, AllocNone);

  swa.border_pixel = 0;
  window = XCreateWindow(dpy, RootWindow(dpy, vInfo->screen),
                       0, 0, 16, 16,
                       0, vInfo->depth, InputOutput, vInfo->visual,
                       CWBorderPixel | CWColormap, &swa);

  ///// Get a GL context and make it current //////
  GLXContext context = glXCreateContext(dpy, vInfo, nullptr, True);
  glXMakeCurrent(dpy, window, context);

  ///// Look for this symbol to determine texture_from_pixmap support /////
  void* glXBindTexImageEXT = glXGetProcAddress("glXBindTexImageEXT"); 

  ///// Get GL vendor/renderer/versions strings /////
  enum { bufsize = 1024 };
  char buf[bufsize];
  const GLubyte *vendorString = glGetString(GL_VENDOR);
  const GLubyte *rendererString = glGetString(GL_RENDERER);
  const GLubyte *versionString = glGetString(GL_VERSION);

  if (!vendorString || !rendererString || !versionString)
    fatal_error("glGetString returned null");

  int length = snprintf(buf, bufsize,
                        "PROVIDER\nGLX\nVENDOR\n%s\nRENDERER\n%s\nVERSION\n%s\nTFP\n%s\n",
                        vendorString,
                        rendererString,
                        versionString,
                        glXBindTexImageEXT ? "TRUE" : "FALSE");
  if (length >= bufsize)
    fatal_error("GL strings length too large for buffer size");

  ///// Clean up. Indeed, the parent process might fail to kill us (e.g. if it doesn't need to check GL info)
  ///// so we might be staying alive for longer than expected, so it's important to consume as little memory as
  ///// possible. Also we want to check that we're able to do that too without generating X errors.
  glXMakeCurrent(dpy, None, nullptr); // must release the GL context before destroying it
  glXDestroyContext(dpy, context);
  XDestroyWindow(dpy, window);
  XFreeColormap(dpy, swa.colormap);

#ifdef NS_FREE_PERMANENT_DATA // conditionally defined in nscore.h, don't forget to #include it above
  XCloseDisplay(dpy);
#else
  // This XSync call wanted to be instead:
  //   XCloseDisplay(dpy);
  // but this can cause 1-minute stalls on certain setups using Nouveau, see bug 973192
  XSync(dpy, False);
#endif

  dlclose(libgl);

  ///// Finally write data to the pipe
  mozilla::Unused << write(write_end_of_the_pipe, buf, length);
}

}

/** \returns true in the child glxtest process, false in the parent process */
bool fire_glxtest_process()
{
  int pfd[2];
  if (pipe(pfd) == -1) {
      perror("pipe");
      return false;
  }
#if MOZ_WIDGET_GTK == 2
  int gtkpfd[2];
  if (pipe(gtkpfd) == -1) {
      perror("pipe");
      return false;
  }
#endif
  pid_t pid = fork();
  if (pid < 0) {
      perror("fork");
      close(pfd[0]);
      close(pfd[1]);
#if MOZ_WIDGET_GTK == 2
      close(gtkpfd[0]);
      close(gtkpfd[1]);
#endif
      return false;
  }
  // The child exits early to avoid running the full shutdown sequence and avoid conflicting with threads 
  // we have already spawned (like the profiler).
  if (pid == 0) {
      close(pfd[0]);
      write_end_of_the_pipe = pfd[1];
#if MOZ_WIDGET_GTK == 2
      close(gtkpfd[0]);
      gtk_write_end_of_the_pipe = gtkpfd[1];
#endif
      glxtest();
      close(pfd[1]);
#if MOZ_WIDGET_GTK == 2
      close(gtkpfd[1]);
#endif
      _exit(0);
  }

  close(pfd[1]);
  mozilla::widget::glxtest_pipe = pfd[0];
  mozilla::widget::glxtest_pid = pid;
#if MOZ_WIDGET_GTK == 2
  close(gtkpfd[1]);
  gtk_read_end_of_the_pipe = gtkpfd[0];
#endif
  return false;
}
