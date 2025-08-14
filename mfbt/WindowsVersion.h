/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsVersion_h
#define mozilla_WindowsVersion_h

#include "mozilla/Attributes.h"
#include <stdint.h>
#include <windows.h>

namespace mozilla {

inline bool
IsWindowsVersionOrLater(uint32_t aVersion)
{
  static uint32_t minVersion = 0;
  static uint32_t maxVersion = UINT32_MAX;

  if (minVersion >= aVersion) {
    return true;
  }

  if (aVersion >= maxVersion) {
    return false;
  }

  OSVERSIONINFOEX info;
  ZeroMemory(&info, sizeof(OSVERSIONINFOEX));
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  info.dwMajorVersion = aVersion >> 24;
  info.dwMinorVersion = (aVersion >> 16) & 0xFF;
  info.wServicePackMajor = (aVersion >> 8) & 0xFF;
  info.wServicePackMinor = aVersion & 0xFF;

  DWORDLONG conditionMask = 0;
  VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(conditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
  VER_SET_CONDITION(conditionMask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);

  if (VerifyVersionInfo(&info,
                        VER_MAJORVERSION | VER_MINORVERSION |
                        VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
                        conditionMask)) {
    minVersion = aVersion;
    return true;
  }

  maxVersion = aVersion;
  return false;
}

inline bool
IsWindowsBuildOrLater(uint32_t aBuild)
{
  static uint32_t minBuild = 0;
  static uint32_t maxBuild = UINT32_MAX;

  if (minBuild >= aBuild) {
    return true;
  }

  if (aBuild >= maxBuild) {
    return false;
  }

  OSVERSIONINFOEX info;
  ZeroMemory(&info, sizeof(OSVERSIONINFOEX));
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  info.dwBuildNumber = aBuild;

  DWORDLONG conditionMask = 0;
  VER_SET_CONDITION(conditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

  if (VerifyVersionInfo(&info, VER_BUILDNUMBER, conditionMask)) {
    minBuild = aBuild;
    return true;
  }

  maxBuild = aBuild;
  return false;
}

MOZ_ALWAYS_INLINE bool
IsXPSP3OrLater()
{
  return IsWindowsVersionOrLater(0x05010300ul);
}

MOZ_ALWAYS_INLINE bool
IsWin2003OrLater()
{
  return IsWindowsVersionOrLater(0x05020000ul);
}

MOZ_ALWAYS_INLINE bool
IsWin2003SP2OrLater()
{
  return IsWindowsVersionOrLater(0x05020200ul);
}

MOZ_ALWAYS_INLINE bool
IsVistaOrLater()
{
  return IsWindowsVersionOrLater(0x06000000ul);
}

MOZ_ALWAYS_INLINE bool
IsVistaSP1OrLater()
{
  return IsWindowsVersionOrLater(0x06000100ul);
}

MOZ_ALWAYS_INLINE bool
IsWin7OrLater()
{
  return IsWindowsVersionOrLater(0x06010000ul);
}

MOZ_ALWAYS_INLINE bool
IsWin7SP1OrLater()
{
  return IsWindowsVersionOrLater(0x06010100ul);
}

MOZ_ALWAYS_INLINE bool
IsWin8OrLater()
{
  return IsWindowsVersionOrLater(0x06020000ul);
}

MOZ_ALWAYS_INLINE bool
IsWin8Point1OrLater()
{
  return IsWindowsVersionOrLater(0x06030000ul);
}

MOZ_ALWAYS_INLINE bool
IsWin10OrLater()
{
  return IsWindowsVersionOrLater(0x0a000000ul);
}

enum class WinBuild : uint32_t {
  Win10RTM = 10240,
  Win10v1511 = 10586,
  Win10v1607 = 14393,
  Win10v1703 = 15063,
  Win10v1709 = 16299,
  Win10v1803 = 17134,
  Win10v1809 = 17763,
  Win10v1903 = 18362,
  Win10v19H2 = 18363,
  Win10v20H1 = 19041,
  Win10v20H2 = 19042,
  Win10v21H1 = 19043,
  Win10v21H2 = 19044,
  Win10v22H2 = 19045,
  Win11RTM = 22000,
  Win11v22H2 = 22621,
  Win11v23H2 = 22631,
  Win11v24H2 = 26100,
  Win11v25H2 = 26200
};
  
// Check for at least named build aBuild taken from WinBuild enum above.
inline bool
IsWindows10BuildOrLater(WinBuild aBuild) {
  uint32_t build = static_cast<uint32_t>(aBuild);
  return IsWin10OrLater() && IsWindowsBuildOrLater(build);
}

// Windows 11 RTM
MOZ_ALWAYS_INLINE bool
IsWin11OrLater()
{
  return IsWindows10BuildOrLater(WinBuild::Win11RTM);
}

MOZ_ALWAYS_INLINE bool
IsNotWin7PreRTM()
{
  return IsWin7SP1OrLater() || !IsWin7OrLater() ||
         IsWindowsBuildOrLater(7600);
}

MOZ_ALWAYS_INLINE bool
IsWin7AndPre2000Compatible() {
  /*
   * See Bug 1279171.
   * We'd like to avoid using WMF on specific OS version when compatibility
   * mode is in effect. The purpose of this function is to check if FF runs on
   * Win7 OS with application compatibility mode being set to 95/98/ME.
   * Those compatibility mode options (95/98/ME) can only display and
   * be selected for 32-bit application.
   * If the compatibility mode is in effect, the GetVersionEx function will
   * report the OS as it identifies itself, which may not be the OS that is
   * installed.
   * Note : 1) We only target for Win7 build number greater than 7600.
   *        2) GetVersionEx may be altered or unavailable for release after
   *           Win8.1. Set pragma to avoid build warning as error.
   */
  bool isWin7 = IsNotWin7PreRTM() && !IsWin8OrLater();
  if (!isWin7) {
    return false;
  }

  OSVERSIONINFOEX info;
  ZeroMemory(&info, sizeof(OSVERSIONINFOEX));

  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
#pragma warning(push)
#pragma warning(disable:4996)
  bool success = GetVersionEx((LPOSVERSIONINFO) &info);
#pragma warning(pop)
  if (!success) {
    return false;
  }
  return info.dwMajorVersion < 5;
}

} // namespace mozilla

#endif /* mozilla_WindowsVersion_h */
