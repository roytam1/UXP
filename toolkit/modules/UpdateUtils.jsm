/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution
#if !MOZ_PKG_SPECIAL
#define MOZ_PKG_SPECIAL false
#endif

this.EXPORTED_SYMBOLS = ["UpdateUtils"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
#ifdef XP_WIN
Cu.import("resource://gre/modules/ctypes.jsm");
Cu.import("resource://gre/modules/WindowsRegistry.jsm");

const WINREG_HKLM                         = Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE;
const WINREG_WINNT                        = "Software\\Microsoft\\Windows NT\\CurrentVersion";
#endif
const PREF_APP_DISTRIBUTION               = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION       = "distribution.version";
const PREF_APP_UPDATE_CHANNEL             = "app.update.channel";
const PREF_APP_UPDATE_CUSTOM              = "app.update.custom";
const PREF_APP_UPDATE_IMEI_HASH           = "app.update.imei_hash";
const PREF_LOCALE                         = "general.useragent.locale";

this.UpdateUtils = {
  get UpdateChannel() {
    return Services.prefs.getDefaultBranch(null)
                         .getCharPref(PREF_APP_UPDATE_CHANNEL, "@MOZ_UPDATE_CHANNEL@");
  },

  get Locale() {
    return Services.prefs.getDefaultBranch(null)
                         .getCharPref(PREF_LOCALE, "en-US");
  },

  /**
   * Formats a URL by replacing %...% values with OS, build and locale specific
   * values.
   *
   * @param  aUpdateURL
   *         The URL to format.
   * @param  aAdditionalSubsts
   * @return The formatted URL.
   */
  formatUpdateURL: function(aUpdateURL, aAdditionalSubsts = null) {  
    // We want to be able to accept additional substs but also to have them be able to
    // override the default ones below
    if (aAdditionalSubsts) {
      try {
        aAdditionalSubsts.forEach(([_subst, _value]) => aUpdateURL = aUpdateURL.replace(_subst, _value));
      } catch(ex) {
        Cu.reportError(ex);
      }
    }

    // appName SHOULD be lower case and not contain spaces even if it has in the past
    let appName = Services.appinfo.name.replace(/\s+/g, '').toLowerCase()

    // We want default pref values if they exist
    let prefBranch = Services.prefs.getDefaultBranch(null)
    let custom = prefBranch.getCharPref(PREF_APP_UPDATE_CUSTOM, "");
    let distribution = prefBranch.getCharPref(PREF_APP_DISTRIBUTION, "default");
    let distributionVersion = prefBranch.getCharPref(PREF_APP_DISTRIBUTION_VERSION, "default");
    let navigator = Services.appShell.hiddenDOMWindow.navigator;

    let substs = [
      [/%ID%/g,                     Services.appinfo.ID],
      [/%PRODUCT%/g,                appName],
      [/%VERSION%/g,                Services.appinfo.version],
      [/%BUILD_ID%/g,               Services.appinfo.appBuildID],
      [/%BUILD_TARGET%/g,           Services.appinfo.OS + "_" + this.ABI],
      [/%BUILD_SPECIAL%/g,          "@MOZ_PKG_SPECIAL@"],
      [/%OS_VERSION%/g,             this.OSVersion],
      [/%WIDGET_TOOLKIT%/g,         "@MOZ_WIDGET_TOOLKIT@"],
      [/%CHANNEL%/g,                this.UpdateChannel],
      [/%CUSTOM%/g,                 custom],
      [/%PLATFORM_VERSION%/g,       Services.appinfo.platformVersion],
      [/%DISTRIBUTION%/g,           distribution],
      [/%DISTRIBUTION_VERSION%/g,   distributionVersion],
      [/%LOCALE%/g,                 this.Locale],
      [/%CPU_SSE2%/g,               navigator.cpuHasSSE2],
      [/%CPU_AVX%/g,                navigator.cpuHasAVX],
      [/%CPU_AVX2%/g,               navigator.cpuHasAVX2]
    ];

    substs.forEach(([_subst, _value]) => aUpdateURL = aUpdateURL.replace(_subst, _value));

    // We do this last to make sure all pluses are converted
    aUpdateURL = aUpdateURL.replace(/\+/g, "%2B");

    return aUpdateURL;
  }
};

#ifdef XP_WIN
/* Windows only getter that returns the processor architecture. */
XPCOMUtils.defineLazyGetter(this, "gWinCPUArch", function aus_gWinCPUArch() {
  // Get processor architecture
  let arch = "unknown";

  const WORD = ctypes.uint16_t;
  const DWORD = ctypes.uint32_t;

  // This structure is described at:
  // http://msdn.microsoft.com/en-us/library/ms724958%28v=vs.85%29.aspx
  const SYSTEM_INFO = new ctypes.StructType('SYSTEM_INFO',
      [
        {wProcessorArchitecture: WORD},
        {wReserved: WORD},
        {dwPageSize: DWORD},
        {lpMinimumApplicationAddress: ctypes.voidptr_t},
        {lpMaximumApplicationAddress: ctypes.voidptr_t},
        {dwActiveProcessorMask: DWORD.ptr},
        {dwNumberOfProcessors: DWORD},
        {dwProcessorType: DWORD},
        {dwAllocationGranularity: DWORD},
        {wProcessorLevel: WORD},
        {wProcessorRevision: WORD}
      ]);

  let kernel32 = false;
  try {
    kernel32 = ctypes.open("Kernel32");
  } catch (ex) {
    Cu.reportError("Unable to open kernel32! Exception: " + ex);
  }

  if (kernel32) {
    try {
      let GetNativeSystemInfo = kernel32.declare("GetNativeSystemInfo",
                                                 ctypes.default_abi,
                                                 ctypes.void_t,
                                                 SYSTEM_INFO.ptr);
      let winSystemInfo = SYSTEM_INFO();
      // Default to unknown
      winSystemInfo.wProcessorArchitecture = 0xffff;

      GetNativeSystemInfo(winSystemInfo.address());
      switch (winSystemInfo.wProcessorArchitecture) {
        case 9:
          arch = "x64";
          break;
        case 6:
          arch = "IA64";
          break;
        case 0:
          arch = "x86";
          break;
      }
    } catch (ex) {
      Cu.reportError("Error getting processor architecture. " +
                     "Exception: " + ex);
    } finally {
      kernel32.close();
    }
  }

  return arch;
});
#endif

XPCOMUtils.defineLazyGetter(UpdateUtils, "ABI", function() {
  let abi = null;
  try {
    abi = Services.appinfo.XPCOMABI;
  } catch (ex) {
    Cu.reportError("XPCOM ABI unknown");
  }

#ifdef XP_MACOSX
  // Mac universal build should report a different ABI than either macppc
  // or mactel.
  let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                 getService(Ci.nsIMacUtils);

  if (macutils.isUniversalBinary) {
    abi += "-u-" + macutils.architecturesInBinary;
  }
#endif

  return abi;
});

XPCOMUtils.defineLazyGetter(UpdateUtils, "OSVersion", function() {
  let osVersion;
  try {
    osVersion = Services.sysinfo.getProperty("name") + " " +
                Services.sysinfo.getProperty("version");
  } catch(ex) {
    Cu.reportError("OS Version unknown.");
  }

#ifdef XP_WIN
  if (osVersion) {
    let currentBuild = WindowsRegistry.readRegKey(WINREG_HKLM, WINREG_WINNT, "CurrentBuild");
    let CSDBuildNumber = WindowsRegistry.readRegKey(WINREG_HKLM, WINREG_WINNT, "CSDBuildNumber");
    
    if (!CSDBuildNumber) {
      CSDBuildNumber  = "0";
    }

    osVersion = osVersion.replace(/Windows_NT/g, "WINNT");
    osVersion += "." + currentBuild + "." + CSDBuildNumber

    // Add processor architecture
    osVersion += " (" + gWinCPUArch + ")";
  }
#endif

  try {
    osVersion += " (" + Services.sysinfo.getProperty("secondaryLibrary") + ")";
  } catch(ex) {
    // Not all platforms have a secondary widget library, so an error is nothing to worry about.
  }

  osVersion = encodeURIComponent(osVersion);

  return osVersion;
});
