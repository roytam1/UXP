/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var UserAgentCompatibility = {
  PREF_UA_COMPAT: "general.useragent.compatMode",
  PREF_UA_COMPAT_GECKO: "general.useragent.compatMode.gecko",
  PREF_UA_COMPAT_FIREFOX: "general.useragent.compatMode.firefox",
  
  init: function() {
    this.checkPreferences();
    Services.prefs.addObserver(this.PREF_UA_COMPAT, this, false);
    Services.prefs.addObserver(this.PREF_UA_COMPAT_GECKO, this, false);
    Services.prefs.addObserver(this.PREF_UA_COMPAT_FIREFOX, this, false);
  },

  uninit: function() {
    Services.prefs.removeObserver(this.PREF_UA_COMPAT, this, false);
    Services.prefs.removeObserver(this.PREF_UA_COMPAT_GECKO, this, false);
    Services.prefs.removeObserver(this.PREF_UA_COMPAT_FIREFOX, this, false);
  },

  observe: function() {
    this.checkPreferences();
  },
  
  checkPreferences: function() {
    var compatMode = Services.prefs.getIntPref(this.PREF_UA_COMPAT);
    
    switch(compatMode) {
      case 0:
        Services.prefs.setBoolPref(this.PREF_UA_COMPAT_GECKO, false);
        Services.prefs.setBoolPref(this.PREF_UA_COMPAT_FIREFOX, false);
        break;
      case 1:
        Services.prefs.setBoolPref(this.PREF_UA_COMPAT_GECKO, true);
        Services.prefs.setBoolPref(this.PREF_UA_COMPAT_FIREFOX, false);
        break;
      case 2:
        Services.prefs.setBoolPref(this.PREF_UA_COMPAT_GECKO, true);
        Services.prefs.setBoolPref(this.PREF_UA_COMPAT_FIREFOX, true);
        break;
    }
  }
};
