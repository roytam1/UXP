/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProviderX11.h"

#include "GLContextProvider.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticMutex.h"
#include "nsDebug.h"
#include "nsString.h"
#include "plstr.h"
#include "prenv.h"

namespace mozilla {
namespace gl {

namespace {

enum class X11ProviderKind : uint8_t {
    Unresolved,
    EGL,
    GLX
};

enum class X11ProviderSelection : uint8_t {
    Auto,
    EGL,
    GLX
};

StaticMutex sProviderMutex;
X11ProviderKind sProviderKind = X11ProviderKind::Unresolved;

static X11ProviderSelection
ParseProviderSelection(const char* aValue)
{
    if (!aValue || !*aValue) {
        return X11ProviderSelection::Auto;
    }

    if (!PL_strcasecmp(aValue, "egl")) {
        return X11ProviderSelection::EGL;
    }

    if (!PL_strcasecmp(aValue, "glx")) {
        return X11ProviderSelection::GLX;
    }

    return X11ProviderSelection::Auto;
}

static X11ProviderSelection
GetProviderSelection()
{
    const char* envValue = PR_GetEnv("MOZ_X11_GL_PROVIDER");
    if (envValue && *envValue) {
        return ParseProviderSelection(envValue);
    }

    nsAdoptingCString prefValue =
        Preferences::GetCString("gfx.x11.gl-provider");
    if (prefValue && !prefValue.IsEmpty()) {
        return ParseProviderSelection(prefValue.get());
    }

    return X11ProviderSelection::Auto;
}

template<typename EGLFn, typename GLXFn>
static already_AddRefed<GLContext>
CreateWithFallback(const char* aOperation,
                   EGLFn&& aCreateEGL,
                   GLXFn&& aCreateGLX)
{
    RefPtr<GLContext> context;
    X11ProviderSelection selection = GetProviderSelection();

    {
        StaticMutexAutoLock lock(sProviderMutex);
        if (selection == X11ProviderSelection::GLX ||
            sProviderKind == X11ProviderKind::GLX)
        {
            sProviderKind = X11ProviderKind::GLX;
            context = aCreateGLX();
            return context.forget();
        }
    }

    context = aCreateEGL();
    if (context) {
        StaticMutexAutoLock lock(sProviderMutex);
        sProviderKind = X11ProviderKind::EGL;
        return context.forget();
    }

    if (selection == X11ProviderSelection::EGL) {
        printf_stderr("GLContextProviderX11: EGL %s failed with provider forced to EGL.\n",
                      aOperation);
        return nullptr;
    }

    printf_stderr("GLContextProviderX11: EGL %s failed, falling back to GLX.\n",
                  aOperation);

    {
        StaticMutexAutoLock lock(sProviderMutex);
        sProviderKind = X11ProviderKind::GLX;
    }

    context = aCreateGLX();
    return context.forget();
}

} // anonymous namespace

already_AddRefed<GLContext>
GLContextProviderX11::CreateForCompositorWidget(widget::CompositorWidget* aCompositorWidget,
                                                bool aForceAccelerated)
{
    return CreateWithFallback(
        "CreateForCompositorWidget",
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderEGL::CreateForCompositorWidget(aCompositorWidget,
                                                                aForceAccelerated));
        },
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderGLX::CreateForCompositorWidget(aCompositorWidget,
                                                                aForceAccelerated));
        });
}

already_AddRefed<GLContext>
GLContextProviderX11::CreateForWindow(nsIWidget* aWidget, bool aForceAccelerated)
{
    return CreateWithFallback(
        "CreateForWindow",
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderEGL::CreateForWindow(aWidget, aForceAccelerated));
        },
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderGLX::CreateForWindow(aWidget, aForceAccelerated));
        });
}

already_AddRefed<GLContext>
GLContextProviderX11::CreateOffscreen(const gfx::IntSize& aSize,
                                      const SurfaceCaps& aMinCaps,
                                      CreateContextFlags aFlags,
                                      nsACString* const aOutFailureId)
{
    return CreateWithFallback(
        "CreateOffscreen",
        [&]() {
            nsAutoCString eglFailureId;
            RefPtr<GLContext> context =
                GLContextProviderEGL::CreateOffscreen(aSize, aMinCaps, aFlags,
                                                      &eglFailureId);
            if (!context && aOutFailureId) {
                aOutFailureId->Assign(eglFailureId);
            }
            return context;
        },
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderGLX::CreateOffscreen(aSize, aMinCaps, aFlags,
                                                      aOutFailureId));
        });
}

already_AddRefed<GLContext>
GLContextProviderX11::CreateHeadless(CreateContextFlags aFlags,
                                     nsACString* const aOutFailureId)
{
    return CreateWithFallback(
        "CreateHeadless",
        [&]() {
            nsAutoCString eglFailureId;
            RefPtr<GLContext> context =
                GLContextProviderEGL::CreateHeadless(aFlags, &eglFailureId);
            if (!context && aOutFailureId) {
                aOutFailureId->Assign(eglFailureId);
            }
            return context;
        },
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderGLX::CreateHeadless(aFlags, aOutFailureId));
        });
}

already_AddRefed<GLContext>
GLContextProviderX11::CreateWrappingExisting(void* aContext, void* aSurface)
{
    return CreateWithFallback(
        "CreateWrappingExisting",
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderEGL::CreateWrappingExisting(aContext, aSurface));
        },
        [&]() {
            return RefPtr<GLContext>(
                GLContextProviderGLX::CreateWrappingExisting(aContext, aSurface));
        });
}

GLContext*
GLContextProviderX11::GetGlobalContext()
{
    X11ProviderSelection selection = GetProviderSelection();

    {
        StaticMutexAutoLock lock(sProviderMutex);
        if (selection == X11ProviderSelection::GLX ||
            sProviderKind == X11ProviderKind::GLX)
        {
            sProviderKind = X11ProviderKind::GLX;
            return GLContextProviderGLX::GetGlobalContext();
        }
    }

    GLContext* context = GLContextProviderEGL::GetGlobalContext();
    if (context) {
        StaticMutexAutoLock lock(sProviderMutex);
        sProviderKind = X11ProviderKind::EGL;
        return context;
    }

    if (selection == X11ProviderSelection::EGL) {
        printf_stderr("GLContextProviderX11: EGL GetGlobalContext failed with provider forced to EGL.\n");
        return nullptr;
    }

    printf_stderr("GLContextProviderX11: EGL GetGlobalContext failed, falling back to GLX.\n");

    {
        StaticMutexAutoLock lock(sProviderMutex);
        sProviderKind = X11ProviderKind::GLX;
    }

    return GLContextProviderGLX::GetGlobalContext();
}

void
GLContextProviderX11::Shutdown()
{
    {
        StaticMutexAutoLock lock(sProviderMutex);
        sProviderKind = X11ProviderKind::Unresolved;
    }

    GLContextProviderEGL::Shutdown();
    GLContextProviderGLX::Shutdown();
}

} // namespace gl
} // namespace mozilla
