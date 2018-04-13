/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Listeners for the DevTools theme.
 */
var DevToolsTheme = {
  _devtoolsThemePrefName: "devtools.theme",
  styleSheet: null,
  initialized: false,

  get isStyleSheetEnabled() {
    return this.styleSheet && !this.styleSheet.sheet.disabled;
  },

  init: function () {
    this.initialized = true;
    Services.prefs.addObserver(this._devtoolsThemePrefName, this, false);
    Services.obs.addObserver(this, "lightweight-theme-styling-update", false);
    Services.obs.addObserver(this, "lightweight-theme-window-updated", false);
    this._updateDevtoolsThemeAttribute();
  },

  observe: function (subject, topic, data) {
    if (topic == "lightweight-theme-styling-update") {
      let newTheme = JSON.parse(data);
      this._toggleStyleSheet();
    }

    if (topic == "nsPref:changed" && data == this._devtoolsThemePrefName) {
      this._updateDevtoolsThemeAttribute();
    }
  },

  _inferBrightness: function() {
    ToolbarIconColor.inferFromText();
    // Get an inverted full screen button if the dark theme is applied.
    if (this.isStyleSheetEnabled &&
        document.documentElement.getAttribute("devtoolstheme") == "dark") {
      document.documentElement.setAttribute("brighttitlebarforeground", "true");
    } else {
      document.documentElement.removeAttribute("brighttitlebarforeground");
    }
  },

  _updateDevtoolsThemeAttribute: function() {
    // Set an attribute on root element to make it possible
    // to change colors based on the selected devtools theme.
    let devtoolsTheme = Services.prefs.getCharPref(this._devtoolsThemePrefName);
    if (devtoolsTheme != "dark") {
      devtoolsTheme = "light";
    }
    document.documentElement.setAttribute("devtoolstheme", devtoolsTheme);
    this._inferBrightness();
  },

  handleEvent: function(e) {
    if (e.type === "load") {
      this.styleSheet.removeEventListener("load", this);
      this.refreshBrowserDisplay();
    }
  },

  refreshBrowserDisplay: function() {
    // Don't touch things on the browser if gBrowserInit.onLoad hasn't
    // yet fired.
    if (this.initialized) {
      gBrowser.tabContainer._positionPinnedTabs();
      this._inferBrightness();
    }
  },

  _toggleStyleSheet: function() {
    let wasEnabled = this.isStyleSheetEnabled;
    if (wasEnabled) {
      this.styleSheet.sheet.disabled = true;
      this.refreshBrowserDisplay();
    }
  },

  uninit: function () {
    Services.prefs.removeObserver(this._devtoolsThemePrefName, this);
    Services.obs.removeObserver(this, "lightweight-theme-styling-update", false);
    Services.obs.removeObserver(this, "lightweight-theme-window-updated", false);
    if (this.styleSheet) {
      this.styleSheet.removeEventListener("load", this);
    }
    this.styleSheet = null;
  }
};
