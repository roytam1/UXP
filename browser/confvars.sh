#! /bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_APP_BASENAME=Basilisk
MOZ_APP_VENDOR=Moonchild
MOZ_PHOENIX=1
MC_BASILISK=1
MOZ_UPDATER=1

if test "$OS_ARCH" = "WINNT" -o \
        "$OS_ARCH" = "Linux"; then
  MOZ_BUNDLED_FONTS=1
fi

if test "$OS_ARCH" = "WINNT"; then
  MOZ_MAINTENANCE_SERVICE=
fi

# Display an error on non-SSE2 Linux systems
if test "$OS_ARCH" = "Linux"; then
  MOZ_LINUX_SSE2_STARTUP_ERROR=1
fi

# For Basilisk we want to use 52.9.YYYY.MM.DD as MOZ_APP_VERSION in release
# builds so add-on developers have something to target while maintaining
# Firefox compatiblity.
# To enable add "export BASILISK_VERSION=1" to the .mozconfig file.
# However, this will cause a full rebuild at 00:00 UTC every day so
# don't export the variable if you are in development or don't care.
# When not exported we fall back the value in the version*.txt file.
if test -n "$BASILISK_VERSION" ; then
    MOZ_APP_VERSION=52.9.`date --utc '+%Y.%m.%d'`
    MOZ_APP_VERSION_DISPLAY=`date --utc '+%Y.%m.%d'`
else
    MOZ_APP_VERSION=`cat ${_topsrcdir}/$MOZ_BUILD_APP/config/version.txt`
    MOZ_APP_VERSION_DISPLAY=`cat ${_topsrcdir}/$MOZ_BUILD_APP/config/version_display.txt`
fi

MOZ_EXTENSIONS_DEFAULT=" gio"

# MOZ_APP_DISPLAYNAME will be set by branding/configure.sh
# MOZ_BRANDING_DIRECTORY is the default branding directory used when none is
# specified. It should never point to the "official" branding directory.
MOZ_BRANDING_DIRECTORY=$MOZ_BUILD_APP/branding/unofficial
MOZ_OFFICIAL_BRANDING_DIRECTORY=$MOZ_BUILD_APP/branding/official
MOZ_APP_ID={ec8030f7-c20a-464f-9b0e-13a3a9e97384}
# This should usually be the same as the value MAR_CHANNEL_ID.
# If more than one ID is needed, then you should use a comma separated list
# of values.
ACCEPTED_MAR_CHANNEL_IDS=basilisk-release
# The MAR_CHANNEL_ID must not contain the following 3 characters: ",\t "
MAR_CHANNEL_ID=basilisk-release

# Features
MOZ_PROFILE_MIGRATOR=1
MOZ_APP_STATIC_INI=1
MOZ_WEBGL_CONFORMANT=1
MOZ_JSDOWNLOADS=1
MOZ_WEBRTC=1
MOZ_WEBEXTENSIONS=1
MOZ_DEVTOOLS=1
MOZ_SERVICES_COMMON=1
MOZ_SERVICES_SYNC=1
MOZ_SERVICES_HEALTHREPORT=
MOZ_SAFE_BROWSING=

# Disable checking that add-ons are signed by the trusted root
MOZ_ADDON_SIGNING=0
MOZ_REQUIRE_SIGNING=0
