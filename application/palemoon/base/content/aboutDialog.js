// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Services = object with smart getters for common XPCOM services
Components.utils.import("resource://gre/modules/Services.jsm");

function init(aEvent)
{
  if (aEvent.target != document) {
    return;
  }

  try {
    var distroId = Services.prefs.getCharPref("distribution.id");
    if (distroId) {
      var distroVersion = Services.prefs.getCharPref("distribution.version");

      var distroIdField = document.getElementById("distributionId");
      distroIdField.value = distroId + " - " + distroVersion;
      distroIdField.style.display = "block";

      try {
        // This is in its own try catch due to bug 895473 and bug 900925.
        var distroAbout = Services.prefs.getComplexValue("distribution.about",
                                                         Components.interfaces.nsISupportsString);
        var distroField = document.getElementById("distribution");
        distroField.value = distroAbout;
        distroField.style.display = "block";
      } catch (ex) {
        // Pref is unset
        Components.utils.reportError(ex);
      }
    }
  } catch(e) {
    // Pref is unset
  }

  // Include the build ID if this is an "a#" or "b#" build
  let version = Services.appinfo.version;
  if (/[ab]\d+$/.test(version)) {
    let buildID = Services.appinfo.appBuildID;
    let buildDate = buildID.slice(0,4) + "-" + buildID.slice(4,6) + "-" + buildID.slice(6,8);
    document.getElementById("aboutVersion").textContent += " (" + buildDate + ")";
  }

// get release notes URL from prefs
  var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                            .getService(Components.interfaces.nsIURLFormatter);
  var releaseNotesURL = formatter.formatURLPref("app.releaseNotesURL");
  if (releaseNotesURL != "about:blank") {
    var relnotes = document.getElementById("releaseNotesURL");
    relnotes.setAttribute("href", releaseNotesURL);
  }
}
