/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGIOService.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsIStringEnumerator.h"
#include "nsAutoPtr.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#ifdef MOZ_ENABLE_DBUS
#include <dbus/dbus.h>
#include "mozilla/ipc/DBusConnectionDelete.h"
#include "mozilla/ipc/DBusMessageRefPtr.h"
#include "mozilla/UniquePtr.h"
#endif

#ifdef MOZ_ENABLE_DBUS
namespace {

bool
IsFileManagerUnavailableError(const DBusError* aError)
{
  return dbus_error_has_name(aError, DBUS_ERROR_SERVICE_UNKNOWN) ||
         dbus_error_has_name(aError, DBUS_ERROR_UNKNOWN_METHOD) ||
         dbus_error_has_name(aError, DBUS_ERROR_UNKNOWN_INTERFACE) ||
         dbus_error_has_name(aError, DBUS_ERROR_UNKNOWN_OBJECT) ||
         dbus_error_has_name(aError, DBUS_ERROR_NAME_HAS_NO_OWNER);
}

bool
IsFileManagerUnavailableReply(DBusMessage* aMessage)
{
  return dbus_message_is_error(aMessage, DBUS_ERROR_SERVICE_UNKNOWN) ||
         dbus_message_is_error(aMessage, DBUS_ERROR_UNKNOWN_METHOD) ||
         dbus_message_is_error(aMessage, DBUS_ERROR_UNKNOWN_INTERFACE) ||
         dbus_message_is_error(aMessage, DBUS_ERROR_UNKNOWN_OBJECT) ||
         dbus_message_is_error(aMessage, DBUS_ERROR_NAME_HAS_NO_OWNER);
}

} // anonymous namespace
#endif


class nsGIOMimeApp final : public nsIGIOMimeApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGIOMIMEAPP

  explicit nsGIOMimeApp(GAppInfo* aApp) : mApp(aApp) {}

private:
  ~nsGIOMimeApp() { g_object_unref(mApp); }

  GAppInfo *mApp;
};

NS_IMPL_ISUPPORTS(nsGIOMimeApp, nsIGIOMimeApp)

NS_IMETHODIMP
nsGIOMimeApp::GetId(nsACString& aId)
{
  aId.Assign(g_app_info_get_id(mApp));
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetName(nsACString& aName)
{
  aName.Assign(g_app_info_get_name(mApp));
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetCommand(nsACString& aCommand)
{
  const char *cmd = g_app_info_get_commandline(mApp);
  if (!cmd)
    return NS_ERROR_FAILURE;
  aCommand.Assign(cmd);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetExpectsURIs(int32_t* aExpects)
{
  *aExpects = g_app_info_supports_uris(mApp);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::Launch(const nsACString& aUri)
{
  GList uris = { 0 };
  nsPromiseFlatCString flatUri(aUri);
  uris.data = const_cast<char*>(flatUri.get());

  GError *error = nullptr;
  gboolean result = g_app_info_launch_uris(mApp, &uris, nullptr, &error);

  if (!result) {
    g_warning("Cannot launch application: %s", error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class GIOUTF8StringEnumerator final : public nsIUTF8StringEnumerator
{
  ~GIOUTF8StringEnumerator() { }

public:
  GIOUTF8StringEnumerator() : mIndex(0) { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUTF8STRINGENUMERATOR

  nsTArray<nsCString> mStrings;
  uint32_t            mIndex;
};

NS_IMPL_ISUPPORTS(GIOUTF8StringEnumerator, nsIUTF8StringEnumerator)

NS_IMETHODIMP
GIOUTF8StringEnumerator::HasMore(bool* aResult)
{
  *aResult = mIndex < mStrings.Length();
  return NS_OK;
}

NS_IMETHODIMP
GIOUTF8StringEnumerator::GetNext(nsACString& aResult)
{
  if (mIndex >= mStrings.Length())
    return NS_ERROR_UNEXPECTED;

  aResult.Assign(mStrings[mIndex]);
  ++mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetSupportedURISchemes(nsIUTF8StringEnumerator** aSchemes)
{
  *aSchemes = nullptr;

  RefPtr<GIOUTF8StringEnumerator> array = new GIOUTF8StringEnumerator();
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

  GVfs *gvfs = g_vfs_get_default();

  if (!gvfs) {
    g_warning("Cannot get GVfs object.");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const gchar* const * uri_schemes = g_vfs_get_supported_uri_schemes(gvfs);

  while (*uri_schemes != nullptr) {
    if (!array->mStrings.AppendElement(*uri_schemes)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    uri_schemes++;
  }

  array.forget(aSchemes);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForMimeType(nsACString const& aMimeType)
{
  char *content_type =
    g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get());
  if (!content_type)
    return NS_ERROR_FAILURE;
  GError *error = nullptr;
  g_app_info_set_as_default_for_type(mApp,
                                     content_type,
                                     &error);
  if (error) {
    g_warning("Cannot set application as default for MIME type (%s): %s",
              PromiseFlatCString(aMimeType).get(),
              error->message);
    g_error_free(error);
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }

  g_free(content_type);
  return NS_OK;
}
/**
 * Set default application for files with given extensions
 * @param fileExts string of space separated extensions
 * @return NS_OK when application was set as default for given extensions,
 * NS_ERROR_FAILURE otherwise
 */
NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForFileExtensions(nsACString const& fileExts)
{
  GError *error = nullptr;
  char *extensions = g_strdup(PromiseFlatCString(fileExts).get());
  char *ext_pos = extensions;
  char *space_pos;

  while ( (space_pos = strchr(ext_pos, ' ')) || (*ext_pos != '\0') ) {
    if (space_pos) {
      *space_pos = '\0';
    }
    g_app_info_set_as_default_for_extension(mApp, ext_pos, &error);
    if (error) {
      g_warning("Cannot set application as default for extension (%s): %s",
                ext_pos,
                error->message);
      g_error_free(error);
      g_free(extensions);
      return NS_ERROR_FAILURE;
    }
    if (space_pos) {
      ext_pos = space_pos + 1;
    } else {
      *ext_pos = '\0';
    }
  }
  g_free(extensions);
  return NS_OK;
}

/**
 * Set default application for URI's of a particular scheme
 * @param aURIScheme string containing the URI scheme
 * @return NS_OK when application was set as default for URI scheme,
 * NS_ERROR_FAILURE otherwise
 */
NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForURIScheme(nsACString const& aURIScheme)
{
  GError *error = nullptr;
  nsAutoCString contentType("x-scheme-handler/");
  contentType.Append(aURIScheme);

  g_app_info_set_as_default_for_type(mApp,
                                     contentType.get(),
                                     &error);
  if (error) {
    g_warning("Cannot set application as default for URI scheme (%s): %s",
              PromiseFlatCString(aURIScheme).get(),
              error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsGIOService, nsIGIOService)

NS_IMETHODIMP
nsGIOService::GetMimeTypeFromExtension(const nsACString& aExtension,
                                             nsACString& aMimeType)
{
  nsAutoCString fileExtToUse("file.");
  fileExtToUse.Append(aExtension);

  gboolean result_uncertain;
  char *content_type = g_content_type_guess(fileExtToUse.get(),
                                            nullptr,
                                            0,
                                            &result_uncertain);
  if (!content_type)
    return NS_ERROR_FAILURE;

  char *mime_type = g_content_type_get_mime_type(content_type);
  if (!mime_type) {
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }

  aMimeType.Assign(mime_type);

  g_free(mime_type);
  g_free(content_type);

  return NS_OK;
}
// used in nsGNOMERegistry
// -----------------------------------------------------------------------------
NS_IMETHODIMP
nsGIOService::GetAppForURIScheme(const nsACString& aURIScheme,
                                 nsIGIOMimeApp** aApp)
{
  *aApp = nullptr;

  GAppInfo *app_info = g_app_info_get_default_for_uri_scheme(
                          PromiseFlatCString(aURIScheme).get());
  if (app_info) {
    nsGIOMimeApp *mozApp = new nsGIOMimeApp(app_info);
    NS_ADDREF(*aApp = mozApp);
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetAppForMimeType(const nsACString& aMimeType,
                                nsIGIOMimeApp**   aApp)
{
  *aApp = nullptr;
  char *content_type =
    g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get());
  if (!content_type)
    return NS_ERROR_FAILURE;

  GAppInfo *app_info = g_app_info_get_default_for_type(content_type, false);
  if (app_info) {
    nsGIOMimeApp *mozApp = new nsGIOMimeApp(app_info);
    NS_ENSURE_TRUE(mozApp, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(*aApp = mozApp);
  } else {
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }
  g_free(content_type);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetDescriptionForMimeType(const nsACString& aMimeType,
                                              nsACString& aDescription)
{
  char *content_type =
    g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get());
  if (!content_type)
    return NS_ERROR_FAILURE;

  char *desc = g_content_type_get_description(content_type);
  if (!desc) {
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }

  aDescription.Assign(desc);
  g_free(content_type);
  g_free(desc);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::ShowURI(nsIURI* aURI)
{
  nsAutoCString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  GError *error = nullptr;
  if (!g_app_info_launch_default_for_uri(spec.get(), nullptr, &error)) {
    g_warning("Could not launch default application for URI: %s", error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::ShowURIForInput(const nsACString& aUri)
{
  GFile *file = g_file_new_for_commandline_arg(PromiseFlatCString(aUri).get());
  char* spec = g_file_get_uri(file);
  nsresult rv = NS_ERROR_FAILURE;
  GError *error = nullptr;

  g_app_info_launch_default_for_uri(spec, nullptr, &error);
  if (error) {
    g_warning("Cannot launch default application: %s", error->message);
    g_error_free(error);
  } else {
    rv = NS_OK;
  }
  g_object_unref(file);
  g_free(spec);

  return rv;
}

NS_IMETHODIMP
nsGIOService::OrgFreedesktopFileManager1ShowItems(const nsACString& aPath)
{
#ifndef MOZ_ENABLE_DBUS
  return NS_ERROR_FAILURE;
#else
  static bool org_freedesktop_FileManager1_exists = true;

  if (!org_freedesktop_FileManager1_exists) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  DBusError error;
  dbus_error_init(&error);

  mozilla::UniquePtr<DBusConnection, mozilla::DBusConnectionDelete>
    connection(dbus_bus_get_private(DBUS_BUS_SESSION, &error));

  if (dbus_error_is_set(&error)) {
    g_printerr("Failed to open connection to session bus: %s\n", error.message);
    dbus_error_free(&error);
    return NS_ERROR_FAILURE;
  }

  if (!connection) {
    return NS_ERROR_FAILURE;
  }

  dbus_connection_set_exit_on_disconnect(connection.get(), false);

  char* uri = g_filename_to_uri(PromiseFlatCString(aPath).get(), nullptr, nullptr);
  if (uri == nullptr) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DBusMessage> msg = already_AddRefed<DBusMessage>(
    dbus_message_new_method_call("org.freedesktop.FileManager1",
                                 "/org/freedesktop/FileManager1",
                                 "org.freedesktop.FileManager1",
                                 "ShowItems"));
  if (!msg) {
    g_free(uri);
    return NS_ERROR_FAILURE;
  }

  const char* startupId = "";
  DBusMessageIter iter;
  DBusMessageIter uris;
  dbus_message_iter_init_append(msg, &iter);

  if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                        DBUS_TYPE_STRING_AS_STRING, &uris) ||
      !dbus_message_iter_append_basic(&uris, DBUS_TYPE_STRING, &uri) ||
      !dbus_message_iter_close_container(&iter, &uris) ||
      !dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &startupId)) {
    g_free(uri);
    return NS_ERROR_FAILURE;
  }

  RefPtr<DBusMessage> reply = already_AddRefed<DBusMessage>(
    dbus_connection_send_with_reply_and_block(connection.get(), msg, -1, &error));
  g_free(uri);

  if (dbus_error_is_set(&error)) {
    bool unavailable = IsFileManagerUnavailableError(&error);
    dbus_error_free(&error);
    if (unavailable) {
      org_freedesktop_FileManager1_exists = false;
      return NS_ERROR_NOT_AVAILABLE;
    }
    return NS_ERROR_FAILURE;
  }

  if (!reply) {
    return NS_ERROR_FAILURE;
  }

  if (IsFileManagerUnavailableReply(reply)) {
    org_freedesktop_FileManager1_exists = false;
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
#endif
}

/**
 * Create or find already existing application info for specified command
 * and application name.
 * @param cmd command to execute
 * @param appName application name
 * @param appInfo location where created GAppInfo is stored
 * @return NS_OK when object is created, NS_ERROR_FAILURE otherwise.
 */
NS_IMETHODIMP
nsGIOService::CreateAppFromCommand(nsACString const& cmd,
                                   nsACString const& appName,
                                   nsIGIOMimeApp**   appInfo)
{
  GError *error = nullptr;
  *appInfo = nullptr;

  GAppInfo *app_info = nullptr, *app_info_from_list = nullptr;
  GList *apps = g_app_info_get_all();
  GList *apps_p = apps;

  // Try to find relevant and existing GAppInfo in all installed application
  // We do this by comparing each GAppInfo's executable with out own
  while (apps_p) {
    app_info_from_list = (GAppInfo*) apps_p->data;
    if (!app_info) {
      // If the executable is not absolute, get it's full path
      char *executable = g_find_program_in_path(g_app_info_get_executable(app_info_from_list));

      if (executable && strcmp(executable, PromiseFlatCString(cmd).get()) == 0) {
        g_object_ref (app_info_from_list);
        app_info = app_info_from_list;
      }
      g_free(executable);
    }

    g_object_unref(app_info_from_list);
    apps_p = apps_p->next;
  }
  g_list_free(apps);

  if (!app_info) {
    app_info = g_app_info_create_from_commandline(PromiseFlatCString(cmd).get(),
                                                  PromiseFlatCString(appName).get(),
                                                  G_APP_INFO_CREATE_SUPPORTS_URIS,
                                                  &error);
  }

  if (!app_info) {
    g_warning("Cannot create application info from command: %s", error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }
  nsGIOMimeApp *mozApp = new nsGIOMimeApp(app_info);
  NS_ENSURE_TRUE(mozApp, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*appInfo = mozApp);
  return NS_OK;
}
