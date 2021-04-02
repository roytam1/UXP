// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var gNewtabUrl = {
  /**
   * Writes browser.newtab.url with the appropriate value.
   * If the choice is "my home page", get and sanitize
   * the browser home page URL to make it suitable for newtab use.
   *
   * Called from prefwindow ondialogaccept in preferences.xul,
   * newtabPage oncommand in tabs.xul, browserHomePage oninput,
   * useCurrent, useBookmark and restoreDefaultHomePage oncommand
   * in main.xul to consider instantApply.
   */
  writeNewtabUrl: function(newtabUrlChoice, browserHomepageUrl) {
    try {
      if (newtabUrlChoice) {
        if (Services.prefs.getBoolPref("browser.preferences.instantApply")) {
          newtabUrlChoice = parseInt(newtabUrlChoice);
        } else {
          return;
        }
      } else {
        if (this.newtabUrlChoiceIsSet) {
          newtabUrlChoice = Services.prefs.getIntPref("browser.newtab.choice");
        } else {
          newtabUrlChoice = this.getNewtabChoice();
        }
      }
      if (browserHomepageUrl || browserHomepageUrl == "") {
        if (Services.prefs.getBoolPref("browser.preferences.instantApply")) {
          if (browserHomepageUrl == "") {
            browserHomepageUrl = "about:home";
          }
        } else {
          return;
        }
      } else {
        browserHomepageUrl = Services.prefs.getComplexValue("browser.startup.homepage",
                              Components.interfaces.nsIPrefLocalizedString).data;
      }
      let newtabUrlPref = Services.prefs.getCharPref("browser.newtab.url");
      switch (newtabUrlChoice) {
        case 1:
          newtabUrlPref = "about:logopage";
          break;
        case 2:
          newtabUrlPref = Services.prefs.getDefaultBranch("browser.")
                            .getComplexValue("startup.homepage",
                              Components.interfaces.nsIPrefLocalizedString).data;
          break;
        case 3:
          // If url is a pipe-delimited set of pages, just take the first one.
          let newtabUrlSanitizedPref=browserHomepageUrl.split("|")[0];
          // XXX: do we need extra sanitation here, e.g. for invalid URLs?
          Services.prefs.setCharPref("browser.newtab.myhome", newtabUrlSanitizedPref);
          newtabUrlPref = newtabUrlSanitizedPref;
          break;
        case 4:
          newtabUrlPref = "about:newtab";
          break;
        default:
          // In case of any other value it's a custom URL, consider instantApply.
          if (this.newtabPageCustom) {
            newtabUrlPref = this.newtabPageCustom;
          }
      } 
      Services.prefs.setCharPref("browser.newtab.url",newtabUrlPref);
    } catch(e) { console.error(e); }
  },

  /**
   * Determines the value of browser.newtab.choice based
   * on the value of browser.newtab.url
   *
   * @returns the value of browser.newtab.choice
   */
  getNewtabChoice: function() {
    let newtabUrlPref = Services.prefs.getCharPref("browser.newtab.url");
    let browserHomepageUrl = Services.prefs.getComplexValue("browser.startup.homepage",
                              Components.interfaces.nsIPrefLocalizedString).data;
    let newtabUrlSanitizedPref = browserHomepageUrl.split("|")[0];
    let defaultStartupHomepage = Services.prefs.getDefaultBranch("browser.")
                                  .getComplexValue("startup.homepage",
                                    Components.interfaces.nsIPrefLocalizedString).data;
    switch (newtabUrlPref) {
      case "about:logopage": 
        return 1;
      case defaultStartupHomepage:
        return 2;
      case newtabUrlSanitizedPref:
        return 3;
      case "about:newtab":
        return 4;
      default: // Custom URL entered.
        // We need this to consider instantApply.
        this.newtabPageCustom = newtabUrlPref;
        return 0;
    }
  }
};
