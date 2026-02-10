/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidgetUtilsGtk.h"

namespace mozilla {

namespace widget {

int32_t WidgetUtilsGTK::IsTouchDeviceSupportPresent()
{
    PointerCapabilities caps;
    GetPointerCapabilities(caps);
    return caps.haveTouchscreen ? 1 : 0;
}

void WidgetUtilsGTK::GetPointerCapabilities(PointerCapabilities& aCaps)
{
    aCaps.haveCoarsePointer = false;
    aCaps.haveFinePointer = false;
    aCaps.haveHoverCapablePointer = false;
    aCaps.haveHoverIncapablePointer = false;
    aCaps.haveTouchscreen = false;

#if GTK_CHECK_VERSION(3,4,0)
    GdkDisplay* display = gdk_display_get_default();
    if (!display) {
        // Assume a mouse.
        aCaps.haveFinePointer = true;
        aCaps.haveHoverCapablePointer = true;
        return;
    }

    GdkDeviceManager* manager = gdk_display_get_device_manager(display);
    if (!manager) {
        // Assume a mouse.
        aCaps.haveFinePointer = true;
        aCaps.haveHoverCapablePointer = true;
        return;
    }

    GList* devices =
        gdk_device_manager_list_devices(manager, GDK_DEVICE_TYPE_SLAVE);
    GList* list = devices;

    while (devices) {
        GdkDevice* device = static_cast<GdkDevice*>(devices->data);
        switch (gdk_device_get_source(device)) {
          case GDK_SOURCE_MOUSE:
          case GDK_SOURCE_TOUCHPAD:
          case GDK_SOURCE_TRACKPOINT:
            aCaps.haveFinePointer = true;
            aCaps.haveHoverCapablePointer = true;
            break;

          case GDK_SOURCE_PEN:
          case GDK_SOURCE_ERASER:
          case GDK_SOURCE_CURSOR:
          case GDK_SOURCE_TABLET_PAD:
            aCaps.haveCoarsePointer = true;
            aCaps.haveHoverIncapablePointer = true;
            break;

          case GDK_SOURCE_TOUCHSCREEN:
            aCaps.haveCoarsePointer = true;
            aCaps.haveHoverIncapablePointer = true;
            aCaps.haveTouchscreen = true;
            break;

          case GDK_SOURCE_KEYBOARD:
            break;
        }
        devices = devices->next;
   }

   if (list) {
       g_list_free(list);
   }
#else
   // Assume a mouse.
   aCaps.haveFinePointer = true;
   aCaps.haveHoverCapablePointer = true;
#endif
}

}  // namespace widget

} // namespace mozilla
