# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

var FullScreen = {
  _XULNS: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",

  toggle: function () {
    var enterFS = window.fullScreen;

    // Toggle the View:FullScreen command, which controls elements like the
    // fullscreen menuitem, menubars, and the appmenu.
    let fullscreenCommand = document.getElementById("View:FullScreen");
    if (enterFS) {
      fullscreenCommand.setAttribute("checked", enterFS);
    } else {
      fullscreenCommand.removeAttribute("checked");
    }

#ifdef XP_MACOSX
    // Make sure the menu items are adjusted.
    document.getElementById("enterFullScreenItem").hidden = enterFS;
    document.getElementById("exitFullScreenItem").hidden = !enterFS;
#endif

    if (!this._fullScrToggler) {
      this._fullScrToggler = document.getElementById("fullscr-toggler");
      this._fullScrToggler.addEventListener("mouseover", this._expandCallback, false);
      this._fullScrToggler.addEventListener("dragenter", this._expandCallback, false);
    }

    // On OS X Lion we don't want to hide toolbars when entering fullscreen, unless
    // we're entering DOM fullscreen, in which case we should hide the toolbars.
    // If we're leaving fullscreen, then we'll go through the exit code below to
    // make sure toolbars are made visible in the case of DOM fullscreen.
    if (enterFS && this.useLionFullScreen) {
      if (document.mozFullScreen) {
        this.showXULChrome("toolbar", false);
      } else {
        gNavToolbox.setAttribute("inFullscreen", true);
        document.documentElement.setAttribute("inFullscreen", true);
      }
      return;
    }

    // show/hide menubars, toolbars (except the full screen toolbar)
    this.showXULChrome("toolbar", !enterFS);

    if (enterFS) {
      document.addEventListener("keypress", this._keyToggleCallback, false);
      document.addEventListener("popupshown", this._setPopupOpen, false);
      document.addEventListener("popuphidden", this._setPopupOpen, false);
      this._shouldAnimate = true;
      if (gPrefService.getBoolPref("browser.fullscreen.autohide")) {
        gBrowser.mPanelContainer.addEventListener("mousemove",
                                                  this._collapseCallback, false);
      }
      // We don't animate the toolbar collapse if in DOM full-screen mode,
      // as the size of the content area would still be changing after the
      // mozfullscreenchange event fired, which could confuse content script.
      this.hideNavToolbox(document.mozFullScreen);
    } else {
      this.showNavToolbox(false);
      // This is needed if they use the context menu to quit fullscreen
      this._isPopupOpen = false;

      document.documentElement.removeAttribute("inDOMFullscreen");

      this.cleanup();
    }
  },

  exitDomFullScreen : function() {
    document.mozCancelFullScreen();
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "activate":
        if (document.mozFullScreen) {
          this.showWarning(this.fullscreenDoc);
        }
        break;
      case "transitionend":
        if (event.propertyName == "opacity") {
          this.cancelWarning();
        }
        break;
    }
  },

  enterDomFullscreen : function(event) {
    if (!document.mozFullScreen) {
      return;
    }

    // However, if we receive a "MozDOMFullscreen:NewOrigin" event for a document
    // which is not a subdocument of a currently active (ie. visible) browser
    // or iframe, we know that we've switched to a different frame since the
    // request to enter full-screen was made, so we should exit full-screen
    // since the "full-screen document" isn't acutally visible.
    if (!event.target.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell).isActive) {
      document.mozCancelFullScreen();
      return;
    }

    let focusManager = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
    if (focusManager.activeWindow != window) {
      // The top-level window has lost focus since the request to enter
      // full-screen was made. Cancel full-screen.
      document.mozCancelFullScreen();
      return;
    }

    document.documentElement.setAttribute("inDOMFullscreen", true);

    if (gFindBarInitialized) {
      gFindBar.close();
    }

    this.showWarning(event.target);

    // Exit DOM full-screen mode upon open, close, or change tab.
    gBrowser.tabContainer.addEventListener("TabOpen", this.exitDomFullScreen);
    gBrowser.tabContainer.addEventListener("TabClose", this.exitDomFullScreen);
    gBrowser.tabContainer.addEventListener("TabSelect", this.exitDomFullScreen);

    // Add listener to detect when the fullscreen window is re-focused.
    // If a fullscreen window loses focus, we show a warning when the
    // fullscreen window is refocused.
    if (!this.useLionFullScreen) {
      window.addEventListener("activate", this);
    }

    // Cancel any "hide the toolbar" animation which is in progress, and make
    // the toolbar hide immediately.
    this.hideNavToolbox(true);
  },

  cleanup: function () {
    if (!window.fullScreen) {
      gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                   this._collapseCallback, false);
      document.removeEventListener("keypress", this._keyToggleCallback, false);
      document.removeEventListener("popupshown", this._setPopupOpen, false);
      document.removeEventListener("popuphidden", this._setPopupOpen, false);

      this.cancelWarning();
      gBrowser.tabContainer.removeEventListener("TabOpen", this.exitDomFullScreen);
      gBrowser.tabContainer.removeEventListener("TabClose", this.exitDomFullScreen);
      gBrowser.tabContainer.removeEventListener("TabSelect", this.exitDomFullScreen);
      if (!this.useLionFullScreen) {
        window.removeEventListener("activate", this);
      }
      this.fullscreenDoc = null;
    }
  },

  // Event callbacks
  _expandCallback: function() {
    FullScreen.showNavToolbox();
  },
  _collapseCallback: function() {
    FullScreen.hideNavToolbox();
  },
  _keyToggleCallback: function(aEvent) {
    // if we can use the keyboard (eg Ctrl+L or Ctrl+E) to open the toolbars, we
    // should provide a way to collapse them too.
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      FullScreen.hideNavToolbox(true);
    } else if (aEvent.keyCode == aEvent.DOM_VK_F6) {
      // F6 is another shortcut to the address bar, but its not covered in OpenLocation()
      FullScreen.showNavToolbox();
    }
  },

  // Checks whether we are allowed to collapse the chrome
  _isPopupOpen: false,
  _isChromeCollapsed: false,
  _safeToCollapse: function(forceHide) {
    if (!gPrefService.getBoolPref("browser.fullscreen.autohide")) {
      return false;
    }

    // a popup menu is open in chrome: don't collapse chrome
    if (!forceHide && this._isPopupOpen) {
      return false;
    }

    // a textbox in chrome is focused (location bar anyone?): don't collapse chrome
    if (document.commandDispatcher.focusedElement &&
        document.commandDispatcher.focusedElement.ownerDocument == document &&
        document.commandDispatcher.focusedElement.localName == "input") {
      if (forceHide) {
        // hidden textboxes that still have focus are bad bad bad
        document.commandDispatcher.focusedElement.blur();
      } else {
        return false;
      }
    }
    return true;
  },

  _setPopupOpen: function(aEvent)
  {
    // Popups should only veto chrome collapsing if they were opened when the chrome was not collapsed.
    // Otherwise, they would not affect chrome and the user would expect the chrome to go away.
    // e.g. we wouldn't want the autoscroll icon firing this event, so when the user
    // toggles chrome when moving mouse to the top, it doesn't go away again.
    if (aEvent.type == "popupshown" && !FullScreen._isChromeCollapsed &&
        aEvent.target.localName != "tooltip" && aEvent.target.localName != "window") {
      FullScreen._isPopupOpen = true;
    } else if (aEvent.type == "popuphidden" && aEvent.target.localName != "tooltip" &&
             aEvent.target.localName != "window") {
      FullScreen._isPopupOpen = false;
    }
  },

  // Autohide helpers for the context menu item
  getAutohide: function(aItem) {
    aItem.setAttribute("checked", gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },
  setAutohide: function() {
    gPrefService.setBoolPref("browser.fullscreen.autohide", !gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },

  // Animate the toolbars disappearing
  _shouldAnimate: true,

  cancelWarning: function(event) {
    if (!this.warningBox) {
      return;
    }
    this.warningBox.removeEventListener("transitionend", this);
    if (this.warningFadeOutTimeout) {
      clearTimeout(this.warningFadeOutTimeout);
      this.warningFadeOutTimeout = null;
    }

    // Ensure focus switches away from the (now hidden) warning box. If the user
    // clicked buttons in the fullscreen key authorization UI, it would have been
    // focused, and any key events would be directed at the (now hidden) chrome
    // document instead of the target document.
    gBrowser.selectedBrowser.focus();

    this.warningBox.setAttribute("hidden", true);
    this.warningBox.removeAttribute("fade-warning-out");
    this.warningBox = null;
  },

  warningBox: null,
  warningFadeOutTimeout: null,
  fullscreenDoc: null,

  // Shows a warning that the site has entered fullscreen for a short duration.
  showWarning: function(targetDoc) {
    let timeout = gPrefService.getIntPref("full-screen-api.warning.timeout");
    if (!document.mozFullScreen || timeout <= 0) {
      return;
    }

    // Set the strings on the fullscreen warning UI.
    this.fullscreenDoc = targetDoc;
    let uri = this.fullscreenDoc.nodePrincipal.URI;
    let host = null;
    try {
      host = uri.host;
    } catch(e) {}
    let hostLabel = document.getElementById("full-screen-domain-text");
    if (host) {
      // Document's principal's URI has a host. Display a warning including the hostname.
      let utils = {};
      Cu.import("resource://gre/modules/DownloadUtils.jsm", utils);
      let displayHost = utils.DownloadUtils.getURIHost(uri.spec)[0];
      let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

      hostLabel.textContent = bundle.formatStringFromName("fullscreen.entered", [displayHost], 1);
      hostLabel.removeAttribute("hidden");
    } else {
      hostLabel.setAttribute("hidden", "true");
    }

    // Note: the warning box can be non-null if the warning box from the previous request
    // wasn't hidden before another request was made.
    if (!this.warningBox) {
      this.warningBox = document.getElementById("full-screen-warning-container");
      // Add a listener to clean up state after the warning is hidden.
      this.warningBox.addEventListener("transitionend", this);
      this.warningBox.removeAttribute("hidden");
    } else {
      if (this.warningFadeOutTimeout) {
        clearTimeout(this.warningFadeOutTimeout);
        this.warningFadeOutTimeout = null;
      }
      this.warningBox.removeAttribute("fade-warning-out");
    }

    // Set a timeout to fade the warning out after a few moments.
    this.warningFadeOutTimeout = setTimeout(() => {
      if (this.warningBox) {
        this.warningBox.setAttribute("fade-warning-out", "true");
      }
    }, timeout);
  },

  showNavToolbox: function(trackMouse = true) {
    this._fullScrToggler.hidden = true;
    gNavToolbox.removeAttribute("fullscreenShouldAnimate");
    gNavToolbox.style.marginTop = "";

    if (!this._isChromeCollapsed) {
      return;
    }

    // Track whether mouse is near the toolbox
    this._isChromeCollapsed = false;
    if (trackMouse) {
      gBrowser.mPanelContainer.addEventListener("mousemove",
                                                this._collapseCallback, false);
    }
  },

  hideNavToolbox: function(forceHide = false) {
    this._fullScrToggler.hidden = document.mozFullScreen;
    if (this._isChromeCollapsed) {
      if (forceHide) {
        gNavToolbox.removeAttribute("fullscreenShouldAnimate");
      }
      return;
    }
    if (!this._safeToCollapse(forceHide)) {
      this._fullScrToggler.hidden = true;
      return;
    }

    // browser.fullscreen.animateUp
    // 0 - never animate up
    // 1 - animate only for first collapse after entering fullscreen (default for perf's sake)
    // 2 - animate every time it collapses
    let animateUp = gPrefService.getIntPref("browser.fullscreen.animateUp");
    if (animateUp == 0) {
      this._shouldAnimate = false;
    } else if (animateUp == 2) {
      this._shouldAnimate = true;
    }
    if (this._shouldAnimate && !forceHide) {
      gNavToolbox.setAttribute("fullscreenShouldAnimate", true);
      this._shouldAnimate = false;
      // Hide the fullscreen toggler until the transition ends.
      let listener = () => {
        gNavToolbox.removeEventListener("transitionend", listener, true);
        if (this._isChromeCollapsed)
          this._fullScrToggler.hidden = false;
      };
      gNavToolbox.addEventListener("transitionend", listener, true);
      this._fullScrToggler.hidden = true;
    }

    gNavToolbox.style.marginTop =
      -gNavToolbox.getBoundingClientRect().height + "px";
    this._isChromeCollapsed = true;
    gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                 this._collapseCallback, false);
  },

  showXULChrome: function(aTag, aShow)
  {
    var els = document.getElementsByTagNameNS(this._XULNS, aTag);

    for (let el of els) {
      // XXX don't interfere with previously collapsed toolbars
      if (el.getAttribute("fullscreentoolbar") == "true") {
        if (!aShow) {

          var toolbarMode = el.getAttribute("mode");
          if (toolbarMode != "text") {
            el.setAttribute("saved-mode", toolbarMode);
            el.setAttribute("saved-iconsize", el.getAttribute("iconsize"));
            el.setAttribute("mode", "icons");
            el.setAttribute("iconsize", "small");
          }

          // Give the main nav bar and the tab bar the fullscreen context menu,
          // otherwise remove context menu to prevent breakage
          el.setAttribute("saved-context", el.getAttribute("context"));
          if (el.id == "nav-bar" || el.id == "TabsToolbar") {
            el.setAttribute("context", "autohide-context");
          } else {
            el.removeAttribute("context");
          }

          // Set the inFullscreen attribute to allow specific styling
          // in fullscreen mode
          el.setAttribute("inFullscreen", true);
        } else {
          var restoreAttr = function restoreAttr(attrName) {
            var savedAttr = "saved-" + attrName;
            if (el.hasAttribute(savedAttr)) {
              el.setAttribute(attrName, el.getAttribute(savedAttr));
              el.removeAttribute(savedAttr);
            }
          }

          restoreAttr("mode");
          restoreAttr("iconsize");
          restoreAttr("context");

          el.removeAttribute("inFullscreen");
        }
      } else {
        // use moz-collapsed so it doesn't persist hidden/collapsed,
        // so that new windows don't have missing toolbars
        if (aShow) {
          el.removeAttribute("moz-collapsed");
        } else {
          el.setAttribute("moz-collapsed", "true");
        }
      }
    }

    if (aShow) {
      gNavToolbox.removeAttribute("inFullscreen");
      document.documentElement.removeAttribute("inFullscreen");
    } else {
      gNavToolbox.setAttribute("inFullscreen", true);
      document.documentElement.setAttribute("inFullscreen", true);
    }

    // In tabs-on-top mode, move window controls to the tab bar,
    // and in tabs-on-bottom mode, move them back to the navigation toolbar.
    // When there is a chance the tab bar may be collapsed, put window
    // controls on nav bar.
    var fullscreenctls = document.getElementById("window-controls");
    var navbar = document.getElementById("nav-bar");
    var ctlsOnTabbar = window.toolbar.visible &&
                       (navbar.collapsed ||
                        (TabsOnTop.enabled &&
                         !gPrefService.getBoolPref("browser.tabs.autoHide")));
    if (fullscreenctls.parentNode == navbar && ctlsOnTabbar) {
      fullscreenctls.removeAttribute("flex");
      document.getElementById("TabsToolbar").appendChild(fullscreenctls);
    } else if (fullscreenctls.parentNode.id == "TabsToolbar" && !ctlsOnTabbar) {
      fullscreenctls.setAttribute("flex", "1");
      navbar.appendChild(fullscreenctls);
    }
    fullscreenctls.hidden = aShow;
    
    ToolbarIconColor.inferFromText();
  }
};
XPCOMUtils.defineLazyGetter(FullScreen, "useLionFullScreen", function() {
  // We'll only use OS X Lion full screen if we're
  // * on OS X
  // * on Lion or higher (Darwin 11+)
  // * have fullscreenbutton="true"
#ifdef XP_MACOSX
  return parseFloat(Services.sysinfo.getProperty("version")) >= 11 &&
         document.documentElement.getAttribute("fullscreenbutton") == "true";
#else
  return false;
#endif
});
