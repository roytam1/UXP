/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// get release notes and vendor URL from prefs
var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                          .getService(Components.interfaces.nsIURLFormatter);
var releaseNotesURL;
try {
  releaseNotesURL = formatter.formatURLPref("app.releaseNotesURL");
} catch(e) {
  releaseNotesURL = "about:blank";
}
if (releaseNotesURL != "about:blank") {
  var relnotes = document.getElementById("releaseNotesURL");
  relnotes.setAttribute("href", releaseNotesURL);
  relnotes.parentNode.removeAttribute("hidden");
}

var vendorURL;
try {
  vendorURL = formatter.formatURLPref("app.vendorURL");
} catch(e) {
  vendorURL = "about:blank";
}
if (vendorURL != "about:blank") {
  var vendor = document.getElementById("vendorURL");
  vendor.setAttribute("href", vendorURL);
}

// insert the version of the XUL application (!= XULRunner platform version)
var versionNum = Components.classes["@mozilla.org/xre/app-info;1"]
                           .getService(Components.interfaces.nsIXULAppInfo)
                           .version;
var version = document.getElementById("version");
#ifdef HAVE_64BIT_BUILD
var versionStr = versionNum + " (64-bit)";
#else
var versionStr = versionNum + " (32-bit)";
#endif
version.textContent += " " + versionStr;

// insert the buildid of the XUL application
var BuildIDVal = Components.classes["@mozilla.org/xre/app-info;1"]
                           .getService(Components.interfaces.nsIXULAppInfo)
                           .appBuildID;
var buildID = document.getElementById("buildID");
buildID.textContent += " " + BuildIDVal.slice(0,-6);

// append user agent
var ua = navigator.userAgent;
if (ua) {
  document.getElementById("userAgent").textContent += " " + ua;
}
