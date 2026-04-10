/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_ENABLE_DBUS

#include "WakeLockListener.h"

#include <dbus/dbus.h>

#include "mozilla/ipc/DBusMessageRefPtr.h"

#define FREEDESKTOP_SCREENSAVER_TARGET    "org.freedesktop.ScreenSaver"
#define FREEDESKTOP_SCREENSAVER_OBJECT    "/ScreenSaver"
#define FREEDESKTOP_SCREENSAVER_INTERFACE "org.freedesktop.ScreenSaver"

#define SESSION_MANAGER_TARGET            "org.gnome.SessionManager"
#define SESSION_MANAGER_OBJECT            "/org/gnome/SessionManager"
#define SESSION_MANAGER_INTERFACE         "org.gnome.SessionManager"

#define DBUS_TIMEOUT                      (-1)

using namespace mozilla;

NS_IMPL_ISUPPORTS(WakeLockListener, nsIDOMMozWakeLockListener)

WakeLockListener* WakeLockListener::sSingleton = nullptr;


enum DesktopEnvironment {
  FreeDesktop,
  GNOME,
  Unsupported,
};

class WakeLockTopic
{
public:
  WakeLockTopic(const nsAString& aTopic, DBusConnection* aConnection)
    : mTopic(NS_ConvertUTF16toUTF8(aTopic))
    , mConnection(aConnection)
    , mDesktopEnvironment(FreeDesktop)
    , mInhibitRequest(0)
    , mShouldInhibit(false)
  {
  }

  nsresult InhibitScreensaver(void);
  nsresult UninhibitScreensaver(void);

private:
  bool SendInhibit();
  bool SendUninhibit();

  bool SendFreeDesktopInhibitMessage();
  bool SendGNOMEInhibitMessage();
  bool SendMessage(DBusMessage* aMessage, uint32_t* aInhibitRequest);
  void InhibitSucceeded(uint32_t aInhibitRequest);

  nsCString mTopic;
  RefPtr<DBusConnection> mConnection;

  DesktopEnvironment mDesktopEnvironment;

  uint32_t mInhibitRequest;

  bool mShouldInhibit;
};


bool
WakeLockTopic::SendMessage(DBusMessage* aMessage, uint32_t* aInhibitRequest)
{
  DBusError error;
  dbus_error_init(&error);

  RefPtr<DBusMessage> reply = already_AddRefed<DBusMessage>(
    dbus_connection_send_with_reply_and_block(mConnection, aMessage,
                                              DBUS_TIMEOUT, &error));
  if (dbus_error_is_set(&error)) {
    dbus_error_free(&error);
    return false;
  }

  if (!reply || dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    return false;
  }

  if (!dbus_message_get_args(reply, nullptr, DBUS_TYPE_UINT32,
                             aInhibitRequest, DBUS_TYPE_INVALID)) {
    return false;
  }

  InhibitSucceeded(*aInhibitRequest);
  return true;
}

bool
WakeLockTopic::SendFreeDesktopInhibitMessage()
{
  RefPtr<DBusMessage> message = already_AddRefed<DBusMessage>(
    dbus_message_new_method_call(FREEDESKTOP_SCREENSAVER_TARGET,
                                 FREEDESKTOP_SCREENSAVER_OBJECT,
                                 FREEDESKTOP_SCREENSAVER_INTERFACE,
                                 "Inhibit"));

  if (!message) {
    return false;
  }

  const char* app = g_get_prgname();
  const char* topic = mTopic.get();
  dbus_message_append_args(message,
                           DBUS_TYPE_STRING, &app,
                           DBUS_TYPE_STRING, &topic,
                           DBUS_TYPE_INVALID);

  uint32_t inhibitRequest;
  return SendMessage(message, &inhibitRequest);
}

bool
WakeLockTopic::SendGNOMEInhibitMessage()
{
  RefPtr<DBusMessage> message = already_AddRefed<DBusMessage>(
    dbus_message_new_method_call(SESSION_MANAGER_TARGET,
                                 SESSION_MANAGER_OBJECT,
                                 SESSION_MANAGER_INTERFACE,
                                 "Inhibit"));

  if (!message) {
    return false;
  }

  static const uint32_t xid = 0;
  static const uint32_t flags = (1 << 3); // Inhibit idle
  const char* app = g_get_prgname();
  const char* topic = mTopic.get();
  dbus_message_append_args(message,
                           DBUS_TYPE_STRING, &app,
                           DBUS_TYPE_UINT32, &xid,
                           DBUS_TYPE_STRING, &topic,
                           DBUS_TYPE_UINT32, &flags,
                           DBUS_TYPE_INVALID);

  uint32_t inhibitRequest;
  return SendMessage(message, &inhibitRequest);
}


bool
WakeLockTopic::SendInhibit()
{
  while (true) {
    switch (mDesktopEnvironment)
    {
    case FreeDesktop:
      if (SendFreeDesktopInhibitMessage()) {
        return true;
      }
      mDesktopEnvironment = GNOME;
      break;
    case GNOME:
      if (SendGNOMEInhibitMessage()) {
        return true;
      }
      mDesktopEnvironment = Unsupported;
      mShouldInhibit = false;
      return false;
    case Unsupported:
      return false;
    }
  }
}

bool
WakeLockTopic::SendUninhibit()
{
  RefPtr<DBusMessage> message;

  if (mDesktopEnvironment == FreeDesktop) {
    message = already_AddRefed<DBusMessage>(
      dbus_message_new_method_call(FREEDESKTOP_SCREENSAVER_TARGET,
                                   FREEDESKTOP_SCREENSAVER_OBJECT,
                                   FREEDESKTOP_SCREENSAVER_INTERFACE,
                                   "UnInhibit"));
  } else if (mDesktopEnvironment == GNOME) {
    message = already_AddRefed<DBusMessage>(
      dbus_message_new_method_call(SESSION_MANAGER_TARGET,
                                   SESSION_MANAGER_OBJECT,
                                   SESSION_MANAGER_INTERFACE,
                                   "Uninhibit"));
  }

  if (!message) {
    return false;
  }

  dbus_message_append_args(message,
                           DBUS_TYPE_UINT32, &mInhibitRequest,
                           DBUS_TYPE_INVALID);

  dbus_connection_send(mConnection, message, nullptr);
  dbus_connection_flush(mConnection);

  mInhibitRequest = 0;

  return true;
}

nsresult
WakeLockTopic::InhibitScreensaver()
{
  if (mShouldInhibit) {
    // Screensaver is inhibited. Nothing to do here.
    return NS_OK;
  }

  mShouldInhibit = true;
  return SendInhibit() ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
WakeLockTopic::UninhibitScreensaver()
{
  if (!mShouldInhibit) {
    // Screensaver isn't inhibited. Nothing to do here.
    return NS_OK;
  }

  mShouldInhibit = false;

  return SendUninhibit() ? NS_OK : NS_ERROR_FAILURE;
}

void
WakeLockTopic::InhibitSucceeded(uint32_t aInhibitRequest)
{
  mInhibitRequest = aInhibitRequest;
}


WakeLockListener::WakeLockListener()
  : mConnection(already_AddRefed<DBusConnection>(
    dbus_bus_get(DBUS_BUS_SESSION, nullptr)))
{
  if (mConnection) {
    dbus_connection_set_exit_on_disconnect(mConnection, false);
  }
}

/* static */ WakeLockListener*
WakeLockListener::GetSingleton(bool aCreate)
{
  if (!sSingleton && aCreate) {
    sSingleton = new WakeLockListener();
    sSingleton->AddRef();
  }

  return sSingleton;
}

/* static */ void
WakeLockListener::Shutdown()
{
  sSingleton->Release();
  sSingleton = nullptr;
}

nsresult
WakeLockListener::Callback(const nsAString& topic, const nsAString& state)
{
  if (!mConnection) {
    return NS_ERROR_FAILURE;
  }

  if(!topic.Equals(NS_LITERAL_STRING("screen")))
    return NS_OK;

  WakeLockTopic* topicLock = mTopics.Get(topic);
  if (!topicLock) {
    topicLock = new WakeLockTopic(topic, mConnection);
    mTopics.Put(topic, topicLock);
  }

  // Treat "locked-background" the same as "unlocked" on desktop linux.
  bool shouldLock = state.EqualsLiteral("locked-foreground");

  return shouldLock ?
    topicLock->InhibitScreensaver() :
    topicLock->UninhibitScreensaver();
}

#endif
