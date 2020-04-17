# -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Services = object with smart getters for common XPCOM services
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Components.utils.import("resource:///modules/RecentWindow.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ShellService",
                                  "resource:///modules/ShellService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
                                  "resource://gre/modules/Deprecated.jsm");

XPCOMUtils.defineLazyGetter(this, "BROWSER_NEW_TAB_URL", function() {
  const PREF = "browser.newtab.url";

  function getNewTabPageURL() {
    if (!Services.prefs.prefHasUserValue(PREF)) {
      if (PrivateBrowsingUtils.isWindowPrivate(window) &&
          !PrivateBrowsingUtils.permanentPrivateBrowsing) {
        return "about:privatebrowsing";
      }
    }
    return Services.prefs.getCharPref(PREF) || "about:blank";
  }

  function update() {
    BROWSER_NEW_TAB_URL = getNewTabPageURL();
  }

  Services.prefs.addObserver(PREF, update, false);

  addEventListener("unload", function onUnload() {
    removeEventListener("unload", onUnload);
    Services.prefs.removeObserver(PREF, update);
  });

  return getNewTabPageURL();
});

var TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";

var gBidiUI = false;

/**
 * Determines whether the given url is considered a special URL for new tabs.
 */
function isBlankPageURL(aURL) {
  // Only make "about:blank", the logopage, or "about:newtab" be 
  // a "blank page" to fix focus issues. 
  // Original code: return aURL == "about:blank" || aURL == BROWSER_NEW_TAB_URL;
  return aURL == "about:blank" || aURL == "about:newtab" || aURL == "about:logopage";
}

function getBrowserURL() {
  return "chrome://browser/content/browser.xul";
}

function getBoolPref(pref, defaultValue) {
  Deprecated.warning("getBoolPref is deprecated and will be removed in a future release. " +
                     "You should use Services.pref.getBoolPref (Services.jsm).",
                     "https://github.com/MoonchildProductions/UXP/issues/989");
  return Services.prefs.getBoolPref(pref, defaultValue);
}


function getTopWin(skipPopups) {
  // If this is called in a browser window, use that window regardless of
  // whether it's the frontmost window, since commands can be executed in
  // background windows (bug 626148).
  if (top.document.documentElement.getAttribute("windowtype") == "navigator:browser" &&
      (!skipPopups || top.toolbar.visible)) {
    return top;
  }

  let isPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
  return RecentWindow.getMostRecentBrowserWindow({ private: isPrivate,
                                                   allowPopups: !skipPopups });
}

function openTopWin(url) {
  /* deprecated */
  openUILinkIn(url, "current");
}

/* openUILink handles clicks on UI elements that cause URLs to load.
 *
 * As the third argument, you may pass an object with the same properties as
 * accepted by openUILinkIn, plus "ignoreButton" and "ignoreAlt".
 */
function openUILink(url, event, aIgnoreButton, aIgnoreAlt, aAllowThirdPartyFixup,
                    aPostData, aReferrerURI) {
  let params;

  if (aIgnoreButton && typeof aIgnoreButton == "object") {
    params = aIgnoreButton;

    // don't forward "ignoreButton" and "ignoreAlt" to openUILinkIn
    aIgnoreButton = params.ignoreButton;
    aIgnoreAlt = params.ignoreAlt;
    delete params.ignoreButton;
    delete params.ignoreAlt;
  } else {
    params = {
      allowThirdPartyFixup: aAllowThirdPartyFixup,
      postData: aPostData,
      referrerURI: aReferrerURI,
      referrerPolicy: Components.interfaces.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
      initiatingDoc: event ? event.target.ownerDocument : null,
    };
  }

  let where = whereToOpenLink(event, aIgnoreButton, aIgnoreAlt);
  openUILinkIn(url, where, params);
}


/* whereToOpenLink() looks at an event to decide where to open a link.
 *
 * The event may be a mouse event (click, double-click, middle-click) or keypress event (enter).
 *
 * On Windows, the modifiers are:
 * Ctrl        new tab, selected
 * Shift       new window
 * Ctrl+Shift  new tab, in background
 * Alt         save
 *
 * Middle-clicking is the same as Ctrl+clicking (it opens a new tab).
 *
 * Exceptions: 
 * - Alt is ignored for menu items selected using the keyboard so you don't accidentally save stuff.  
 *    (Currently, the Alt isn't sent here at all for menu items, but that will change in bug 126189.)
 * - Alt is hard to use in context menus, because pressing Alt closes the menu.
 * - Alt can't be used on the bookmarks toolbar because Alt is used for "treat this as something draggable".
 * - The button is ignored for the middle-click-paste-URL feature, since it's always a middle-click.
 */
function whereToOpenLink(e, ignoreButton, ignoreAlt) {
  // This method must treat a null event like a left click without modifier keys (i.e.
  // e = { shiftKey:false, ctrlKey:false, metaKey:false, altKey:false, button:0 })
  // for compatibility purposes.
  if (!e) {
    return "current";
  }

  var shift = e.shiftKey;
  var ctrl =  e.ctrlKey;
  var meta =  e.metaKey;
  var alt  =  e.altKey && !ignoreAlt;

  // ignoreButton allows "middle-click paste" to use function without always opening in a new window.
  var middle = !ignoreButton && e.button == 1;
  var middleUsesTabs = Services.prefs.getBoolPref("browser.tabs.opentabfor.middleclick", true);

  // Don't do anything special with right-mouse clicks.  They're probably clicks on context menu items.

#ifdef XP_MACOSX
  if (meta || (middle && middleUsesTabs)) {
#else
  if (ctrl || (middle && middleUsesTabs)) {
#endif
    return shift ? "tabshifted" : "tab";
  }

  if (alt && Services.prefs.getBoolPref("browser.altClickSave", false)) {
    return "save";
  }

  if (shift || (middle && !middleUsesTabs)) {
    return "window";
  }

  return "current";
}

/* openUILinkIn opens a URL in a place specified by the parameter |where|.
 *
 * |where| can be:
 *  "current"     current tab            (if there aren't any browser windows, then in a new window instead)
 *  "tab"         new tab                (if there aren't any browser windows, then in a new window instead)
 *  "tabshifted"  same as "tab" but in background if default is to select new tabs, and vice versa
 *  "window"      new window
 *  "save"        save to disk (with no filename hint!)
 *
 * aAllowThirdPartyFixup controls whether third party services such as Google's
 * I Feel Lucky are allowed to interpret this URL. This parameter may be
 * undefined, which is treated as false.
 *
 * Instead of aAllowThirdPartyFixup, you may also pass an object with any of
 * these properties:
 *   allowThirdPartyFixup (boolean)
 *   postData             (nsIInputStream)
 *   referrerURI          (nsIURI)
 *   relatedToCurrent     (boolean)
 */
function openUILinkIn(url, where, aAllowThirdPartyFixup, aPostData, aReferrerURI) {
  var params;

  if (arguments.length == 3 && typeof arguments[2] == "object") {
    params = aAllowThirdPartyFixup;
  } else {
    params = {
      allowThirdPartyFixup: aAllowThirdPartyFixup,
      postData: aPostData,
      referrerURI: aReferrerURI,
      referrerPolicy: Components.interfaces.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
    };
  }

  params.fromChrome = true;

  openLinkIn(url, where, params);
}

/* eslint-disable complexity */
function openLinkIn(url, where, params) {
  if (!where || !url) {
    return;
  }
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  var aFromChrome           = params.fromChrome;
  var aAllowThirdPartyFixup = params.allowThirdPartyFixup;
  var aPostData             = params.postData;
  var aCharset              = params.charset;
  var aReferrerURI          = params.referrerURI;
  var aReferrerPolicy       = ('referrerPolicy' in params ?
                               params.referrerPolicy :
                               Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT);
  var aRelatedToCurrent     = params.relatedToCurrent;
  var aForceAllowDataURI    = params.forceAllowDataURI;
  var aInBackground         = params.inBackground;
  var aDisallowInheritPrincipal = params.disallowInheritPrincipal;
  var aInitiatingDoc        = params.initiatingDoc;
  var aIsPrivate            = params.private;
  var aPrincipal            = params.originPrincipal;
  var aTriggeringPrincipal  = params.triggeringPrincipal;
  var aForceAboutBlankViewerInCurrent = params.forceAboutBlankViewerInCurrent;
  var sendReferrerURI       = true;

  if (where == "save") {
    if (!aInitiatingDoc) {
      Components.utils.reportError("openUILink/openLinkIn was called with " +
        "where == 'save' but without initiatingDoc.  See bug 814264.");
      return;
    }
    // TODO(1073187): propagate referrerPolicy.
    saveURL(url, null, null, true, null, aReferrerURI, aInitiatingDoc);
    return;
  }

  var w = getTopWin();
  if ((where == "tab" || where == "tabshifted") &&
      w && !w.toolbar.visible) {
    w = getTopWin(true);
    aRelatedToCurrent = false;
  }

  // We can only do this after we're sure of what |w| will be the rest of this function.
  // Note that if |w| is null we might have no current browser (we'll open a new window).
  var aCurrentBrowser = params.currentBrowser || (w && w.gBrowser.selectedBrowser);
  
  // Teach the principal about the right OA to use, e.g. in case when
  // opening a link in a new private window.
  // Please note we do not have to do that for SystemPrincipals and we
  // can not do it for NullPrincipals since NullPrincipals are only
  // identical if they actually are the same object (See Bug: 1346759)
  function useOAForPrincipal(principal) {
    if (principal && principal.isCodebasePrincipal) {
      let attrs = {
        privateBrowsingId: aIsPrivate || (w && PrivateBrowsingUtils.isWindowPrivate(w)),
      };
      return Services.scriptSecurityManager.createCodebasePrincipal(principal.URI, attrs);
    }
    return principal;
  }
  aPrincipal = useOAForPrincipal(aPrincipal);
  aTriggeringPrincipal = useOAForPrincipal(aTriggeringPrincipal);

  if (!w || where == "window") {
    // This propagates to window.arguments.
    // Strip referrer data when opening a new private window, to prevent
    // regular browsing data from leaking into it.
    if (aIsPrivate) {
      sendReferrerURI = false;
    }
    
    var sa = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);

    var wuri = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    wuri.data = url;

    let charset = null;
    if (aCharset) {
      charset = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
      charset.data = "charset=" + aCharset;
    }

    var allowThirdPartyFixupSupports = Cc["@mozilla.org/supports-PRBool;1"]
                                         .createInstance(Ci.nsISupportsPRBool);
    allowThirdPartyFixupSupports.data = aAllowThirdPartyFixup;

    var referrerURISupports = null;
    if (aReferrerURI && sendReferrerURI) {
      referrerURISupports = Cc["@mozilla.org/supports-string;1"]
                              .createInstance(Ci.nsISupportsString);
      referrerURISupports.data = aReferrerURI.spec;
    }

    var referrerPolicySupports = Cc["@mozilla.org/supports-PRUint32;1"]
                                   .createInstance(Ci.nsISupportsPRUint32);
    referrerPolicySupports.data = aReferrerPolicy;

    sa.AppendElement(wuri);
    sa.AppendElement(charset);
    sa.AppendElement(referrerURISupports);
    sa.AppendElement(aPostData);
    sa.AppendElement(allowThirdPartyFixupSupports);
    sa.AppendElement(referrerPolicySupports);
    sa.AppendElement(aPrincipal);
    sa.AppendElement(aTriggeringPrincipal);

    let features = "chrome,dialog=no,all";
    if (aIsPrivate) {
      features += ",private";
    }

    Services.ww.openWindow(w || window, getBrowserURL(), null, features, sa);
    return;
  }

  let loadInBackground = where == "current" ? false : aInBackground;
  if (loadInBackground == null) {
    loadInBackground = aFromChrome ?
                       false :
                       Services.prefs.getBoolPref("browser.tabs.loadInBackground");
  }

  let uriObj;
  if (where == "current") {
    try {
      uriObj = Services.io.newURI(url, null, null);
    } catch(e) {}
  }

  if (where == "current" && w.gBrowser.selectedTab.pinned) {
    try {
      // nsIURI.host can throw for non-nsStandardURL nsIURIs.
      if (!uriObj || !uriObj.schemeIs("javascript") &&
          w.gBrowser.currentURI.host != uriObj.host) {
        where = "tab";
        loadInBackground = false;
      }
    } catch(err) {
      where = "tab";
      loadInBackground = false;
    }
  }

  // Raise the target window before loading the URI, since loading it may
  // result in a new frontmost window (e.g. "javascript:window.open('');").
  w.focus();

  let browserUsedForLoad = null;
  switch (where) {
    case "current":
      let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      if (aAllowThirdPartyFixup) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
      }
      if (aDisallowInheritPrincipal) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
      }
      if (aForceAllowDataURI) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
      }
      let {URI_INHERITS_SECURITY_CONTEXT} = Ci.nsIProtocolHandler;
      if (aForceAboutBlankViewerInCurrent &&
          (!uriObj ||
           (Services.io.getProtocolFlags(uriObj.scheme) & URI_INHERITS_SECURITY_CONTEXT))) {
        // Unless we know for sure we're not inheriting principals,
        // force the about:blank viewer to have the right principal:
        w.gBrowser.selectedBrowser.createAboutBlankContentViewer(aPrincipal);
      }

      w.gBrowser.loadURIWithFlags(url, { flags: flags,
                                         triggeringPrincipal: aTriggeringPrincipal,
                                         referrerURI: aReferrerURI,
                                         referrerPolicy: aReferrerPolicy,
                                         postData: aPostData,
                                         originPrincipal: aPrincipal }); 
      browserUsedForLoad = aCurrentBrowser;
      break;
    case "tabshifted":
      loadInBackground = !loadInBackground;
      // fall through
    case "tab":
      let browser = w.gBrowser;
      let tabUsedForLoad = browser.loadOneTab(url, { referrerURI: aReferrerURI,
                                                     referrerPolicy: aReferrerPolicy,
                                                     charset: aCharset,
                                                     postData: aPostData,
                                                     inBackground: loadInBackground,
                                                     allowThirdPartyFixup: aAllowThirdPartyFixup,
                                                     relatedToCurrent: aRelatedToCurrent,
                                                     originPrincipal: aPrincipal,
                                                     triggeringPrincipal: aTriggeringPrincipal });
      browserUsedForLoad = tabUsedForLoad.linkedBrowser;
      break;
  }

  // Focus the content, but only if the browser used for the load is selected.
  if (browserUsedForLoad &&
      browserUsedForLoad == browserUsedForLoad.getTabBrowser().selectedBrowser) {
    browserUsedForLoad.focus();
  }

  if (!loadInBackground && w.isBlankPageURL(url)) {
    if (!w.focusAndSelectUrlBar()) {
      console.error("Unable to focus and select address bar.")
    }
  }
}

// Used as an onclick handler for UI elements with link-like behavior.
// e.g. onclick="checkForMiddleClick(this, event);"
function checkForMiddleClick(node, event) {
  // We should be using the disabled property here instead of the attribute,
  // but some elements that this function is used with don't support it (e.g.
  // menuitem).
  if (node.getAttribute("disabled") == "true") {
     // Do nothing
    return;
  }

  if (event.button == 1) {
    /* Execute the node's oncommand or command.
     *
     * XXX: we should use node.oncommand(event) once bug 246720 is fixed.
     */
    var target = node.hasAttribute("oncommand") ?
                 node :
                 node.ownerDocument.getElementById(node.getAttribute("command"));
    var fn = new Function("event", target.getAttribute("oncommand"));
    fn.call(target, event);

    // If the middle-click was on part of a menu, close the menu.
    // (Menus close automatically with left-click but not with middle-click.)
    closeMenus(event.target);
  }
}

// Closes all popups that are ancestors of the node.
function closeMenus(node)
{
  if ("tagName" in node) {
    if (node.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" &&
        (node.tagName == "menupopup" || node.tagName == "popup")) {
      node.hidePopup();
    }

    closeMenus(node.parentNode);
  }
}

// Gather all descendant text under given document node.
function gatherTextUnder(root) {
  var text = "";
  var node = root.firstChild;
  var depth = 1;
  while ( node && depth > 0 ) {
    // See if this node is text.
    if ( node.nodeType == Node.TEXT_NODE ) {
      // Add this text to our collection.
      text += " " + node.data;
    } else if ( node instanceof HTMLImageElement) {
      // If it has an alt= attribute, use that.
      var altText = node.getAttribute( "alt" );
      if ( altText && altText != "" ) {
        text = altText;
        break;
      }
    }
    // Find next node to test.
    // First, see if this node has children.
    if ( node.hasChildNodes() ) {
      // Go to first child.
      node = node.firstChild;
      depth++;
    } else {
      // No children, try next sibling (or parent next sibling).
      while ( depth > 0 && !node.nextSibling ) {
        node = node.parentNode;
        depth--;
      }
      if ( node.nextSibling ) {
        node = node.nextSibling;
      }
    }
  }
  // Strip leading whitespace.
  text = text.replace( /^\s+/, "" );
  // Strip trailing whitespace.
  text = text.replace( /\s+$/, "" );
  // Compress remaining whitespace.
  text = text.replace( /\s+/g, " " );
  return text;
}

// This function exists for legacy reasons.
function getShellService() {
  return ShellService;
}

function isBidiEnabled() {
  // first check the pref.
  if (Services.prefs.getBoolPref("bidi.browser.ui", false)) {
    return true;
  }

  // if the pref isn't set, check for an RTL locale and force the pref to true
  // if we find one.
  var rv = false;

  try {
    var localeService = Components.classes["@mozilla.org/intl/nslocaleservice;1"]
                                  .getService(Components.interfaces.nsILocaleService);
    var systemLocale = localeService.getSystemLocale().getCategory("NSILOCALE_CTYPE").substr(0,3);

    switch (systemLocale) {
      case "ar-":
      case "he-":
      case "fa-":
      case "ur-":
      case "syr":
        rv = true;
        Services.prefs.setBoolPref("bidi.browser.ui", true);
    }
  } catch(e) {}

  return rv;
}

#ifdef MOZ_UPDATER
/**
 * Opens the update manager and checks for updates to the application.
 */
function checkForUpdates() {
  var um = Components.classes["@mozilla.org/updates/update-manager;1"]
                     .getService(Components.interfaces.nsIUpdateManager);
  var prompter = Components.classes["@mozilla.org/updates/update-prompt;1"]
                           .createInstance(Components.interfaces.nsIUpdatePrompt);

  // If there's an update ready to be applied, show the "Update Downloaded"
  // UI instead and let the user know they have to restart the application for
  // the changes to be applied.
  if (um.activeUpdate && ["pending", "pending-elevate", "applied"].includes(um.activeUpdate.state)) {
    prompter.showUpdateDownloaded(um.activeUpdate);
  } else {
    prompter.checkForUpdates();
  }
}
#endif

/**
 * Set up the help menu software update items to show proper status,
 * also disabling the items if update is disabled.
 */
function buildHelpMenu() {
#ifdef MOZ_UPDATER
  var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                          .getService(Components.interfaces.nsIApplicationUpdateService);
  var um = Components.classes["@mozilla.org/updates/update-manager;1"]
                     .getService(Components.interfaces.nsIUpdateManager);

  // Disable the UI if the update enabled pref has been locked by the
  // administrator or if we cannot update for some other reason.
  var checkForUpdates = document.getElementById("checkForUpdates");
  var appMenuCheckForUpdates = document.getElementById("appmenu_checkForUpdates");
  var canCheckForUpdates = updates.canCheckForUpdates;
  
  checkForUpdates.setAttribute("disabled", !canCheckForUpdates);

  if (appMenuCheckForUpdates) {
    appMenuCheckForUpdates.setAttribute("disabled", !canCheckForUpdates);
  }

  if (!canCheckForUpdates) {
    return;
  }

  var strings = document.getElementById("bundle_browser");
  var activeUpdate = um.activeUpdate;

  // If there's an active update, substitute its name into the label
  // we show for this item, otherwise display a generic label.
  function getStringWithUpdateName(key) {
    if (activeUpdate && activeUpdate.name) {
      return strings.getFormattedString(key, [activeUpdate.name]);
    }
    return strings.getString(key + "Fallback");
  }

  // By default, show "Check for Updates..." from updatesItem_default or
  // updatesItem_defaultFallback
  var key = "default";
  if (activeUpdate) {
    switch (activeUpdate.state) {
      case "downloading":
        // If we're downloading an update at present, show the text:
        // "Downloading Thunderbird x.x..." from updatesItem_downloading or
        // updatesItem_downloadingFallback, otherwise we're paused, and show
        // "Resume Downloading Thunderbird x.x..." from updatesItem_resume or
        // updatesItem_resumeFallback
        key = updates.isDownloading ? "downloading" : "resume";
        break;
      case "pending":
        // If we're waiting for the user to restart, show: "Apply Downloaded
        // Updates Now..." from updatesItem_pending or
        // updatesItem_pendingFallback
        key = "pending";
        break;
    }
  }

  checkForUpdates.label = getStringWithUpdateName("updatesItem_" + key);

  if (appMenuCheckForUpdates) {
    appMenuCheckForUpdates.label = getStringWithUpdateName("updatesItem_" + key);
  }

  // updatesItem_default.accesskey, updatesItem_downloading.accesskey,
  // updatesItem_resume.accesskey or updatesItem_pending.accesskey
  checkForUpdates.accessKey = strings.getString("updatesItem_" + key + ".accesskey");

  if (appMenuCheckForUpdates) {
    appMenuCheckForUpdates.accessKey = strings.getString("updatesItem_" + key + ".accesskey");
  }

  if (um.activeUpdate && updates.isDownloading) {
    checkForUpdates.setAttribute("loading", "true");
    if (appMenuCheckForUpdates) {
      appMenuCheckForUpdates.setAttribute("loading", "true");
    }
  } else {
    checkForUpdates.removeAttribute("loading");
    if (appMenuCheckForUpdates) {
      appMenuCheckForUpdates.removeAttribute("loading");
    }
  }
#else
#ifndef XP_MACOSX
  // Some extensions may rely on these being present so only hide the about
  // separator when there are no elements besides the check for updates menuitem
  // in between the about separator and the updates separator.
  var updatesSeparator = document.getElementById("updatesSeparator");
  var aboutSeparator = document.getElementById("aboutSeparator");
  var checkForUpdates = document.getElementById("checkForUpdates");
  if (updatesSeparator.nextSibling === checkForUpdates &&
      checkForUpdates.nextSibling === aboutSeparator) {
    updatesSeparator.hidden = true;
  }
#endif
#endif
}


function openAboutDialog() {
  var enumerator = Services.wm.getEnumerator("Browser:About");
  while (enumerator.hasMoreElements()) {
    // Only open one about window (Bug 599573)
    let win = enumerator.getNext();
    win.focus();
    return;
  }

#ifdef XP_WIN
  var features = "chrome,centerscreen,dependent";
#elifdef XP_MACOSX
  var features = "chrome,resizable=no,minimizable=no";
#else
  var features = "chrome,centerscreen,dependent,dialog=no";
#endif
  window.openDialog("chrome://browser/content/aboutDialog.xul", "", features);
}

function openPreferences(paneID, extraArgs) {
  var instantApply = Services.prefs.getBoolPref("browser.preferences.instantApply", false);
  var features = "chrome,titlebar,toolbar,centerscreen" + (instantApply ? ",dialog=no" : ",modal");

  var win = Services.wm.getMostRecentWindow("Browser:Preferences");
  if (win) {
    win.focus();
    if (paneID) {
      var pane = win.document.getElementById(paneID);
      win.document.documentElement.showPane(pane);
    }

    if (extraArgs && extraArgs["advancedTab"]) {
      var advancedPaneTabs = win.document.getElementById("advancedPrefs");
      advancedPaneTabs.selectedTab = win.document.getElementById(extraArgs["advancedTab"]);
    }

    return win;
  }

  return openDialog("chrome://browser/content/preferences/preferences.xul",
                    "Preferences", features, paneID, extraArgs);
}

function openAdvancedPreferences(tabID) {
  openPreferences("paneAdvanced", { "advancedTab" : tabID });
}

/**
 * Opens the release notes page for this version of the application.
 */
function openReleaseNotes() { 
  var relnotesURL = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                              .getService(Components.interfaces.nsIURLFormatter)
                              .formatURLPref("app.releaseNotesURL");
  
  openUILinkIn(relnotesURL, "tab");
}

/**
 * Opens the troubleshooting information (about:support) page for this version
 * of the application.
 */
function openTroubleshootingPage() {
  openUILinkIn("about:support", "tab");
}

/**
 * Opens the feedback page for this version of the application.
 */
function openFeedbackPage() {
  openUILinkIn(Services.prefs.getCharPref("browser.feedback.url"), "tab");
}

function isElementVisible(aElement) {
  if (!aElement) {
    return false;
  }

  // If aElement or a direct or indirect parent is hidden or collapsed,
  // height, width or both will be 0.
  var bo = aElement.boxObject;
  return (bo.height > 0 && bo.width > 0);
}

function makeURLAbsolute(aBase, aUrl)
{
  // Note:  makeURI() will throw if aUri is not a valid URI
  return makeURI(aUrl, null, makeURI(aBase)).spec;
}


/**
 * openNewTabWith: opens a new tab with the given URL.
 *
 * @param aURL
 *        The URL to open (as a string).
 * @param aDocument
 *        The document from which the URL came, or null. This is used to set the
 *        referrer header and to do a security check of whether the document is
 *        allowed to reference the URL. If null, there will be no referrer
 *        header and no security check.
 * @param aPostData
 *        Form POST data, or null.
 * @param aEvent
 *        The triggering event (for the purpose of determining whether to open
 *        in the background), or null.
 * @param aAllowThirdPartyFixup
 *        If true, then we allow the URL text to be sent to third party services
 *        (e.g., Google's I Feel Lucky) for interpretation. This parameter may
 *        be undefined in which case it is treated as false.
 * @param [optional] aReferrer
 *        If aDocument is null, then this will be used as the referrer.
 *        There will be no security check.
 * @param [optional] aReferrerPolicy
 *        Referrer policy - Ci.nsIHttpChannel.REFERRER_POLICY_*.
 */ 
function openNewTabWith(aURL, aDocument, aPostData, aEvent,
                        aAllowThirdPartyFixup, aReferrer, aReferrerPolicy) {
  if (aDocument) {
    urlSecurityCheck(aURL, aDocument.nodePrincipal);
  }

  // As in openNewWindowWith(), we want to pass the charset of the
  // current document over to a new tab.
  var originCharset = aDocument && aDocument.characterSet;
  if (!originCharset &&
      document.documentElement.getAttribute("windowtype") == "navigator:browser") {
    originCharset = window.content.document.characterSet;
  }

  openLinkIn(aURL, aEvent && aEvent.shiftKey ? "tabshifted" : "tab",
             { charset: originCharset,
               postData: aPostData,
               allowThirdPartyFixup: aAllowThirdPartyFixup,
               referrerURI: aDocument ? aDocument.documentURIObject : aReferrer,
               referrerPolicy: aReferrerPolicy,
             });
}

function openNewWindowWith(aURL, aDocument, aPostData, aAllowThirdPartyFixup,
                           aReferrer, aReferrerPolicy) {
  if (aDocument) {
    urlSecurityCheck(aURL, aDocument.nodePrincipal);
  }

  // if and only if the current window is a browser window and it has a
  // document with a character set, then extract the current charset menu
  // setting from the current document and use it to initialize the new browser
  // window...
  var originCharset = aDocument && aDocument.characterSet;
  if (!originCharset &&
      document.documentElement.getAttribute("windowtype") == "navigator:browser") {
    originCharset = window.content.document.characterSet;
  }

  openLinkIn(aURL, "window",
             { charset: originCharset,
               postData: aPostData,
               allowThirdPartyFixup: aAllowThirdPartyFixup,
               referrerURI: aDocument ? aDocument.documentURIObject : aReferrer,
               referrerPolicy: aReferrerPolicy,
             });
}

/**
 * isValidFeed: checks whether the given data represents a valid feed.
 *
 * @param  aLink
 *         An object representing a feed with title, href and type.
 * @param  aPrincipal
 *         The principal of the document, used for security check.
 * @param  aIsFeed
 *         Whether this is already a known feed or not, if true only a security
 *         check will be performed.
 */ 
function isValidFeed(aLink, aPrincipal, aIsFeed) {
  if (!aLink || !aPrincipal) {
    return false;
  }

  var type = aLink.type.toLowerCase().replace(/^\s+|\s*(?:;.*)?$/g, "");
  if (!aIsFeed) {
    aIsFeed = (type == "application/rss+xml" ||
               type == "application/atom+xml");
  }

  if (aIsFeed) {
    try {
      urlSecurityCheck(aLink.href, aPrincipal,
                       Components.interfaces.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
      return type || "application/rss+xml";
    } catch(ex) {}
  }

  return null;
}

// aCalledFromModal is optional
function openHelpLink(aHelpTopic, aCalledFromModal) {
  var url = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                      .getService(Components.interfaces.nsIURLFormatter)
                      .formatURLPref("app.support.baseURL");
  url += aHelpTopic;

  var where = aCalledFromModal ? "window" : "tab";
  openUILinkIn(url, where);
}

function openPrefsHelp() {
  // non-instant apply prefwindows are usually modal, so we can't open in the topmost window, 
  // since its probably behind the window.
  var instantApply = Services.prefs.getBoolPref("browser.preferences.instantApply");

  var helpTopic = document.getElementsByTagName("prefwindow")[0].currentPane.helpTopic;
  openHelpLink(helpTopic, !instantApply);
}

function trimURL(aURL) {
  // This function must not modify the given URL such that calling
  // nsIURIFixup::createFixupURI with the result will produce a different URI.
  return aURL /* remove single trailing slash for http/https/ftp URLs */
             .replace(/^((?:http|https|ftp):\/\/[^/]+)\/$/, "$1")
              /* remove http:// unless the host starts with "ftp\d*\." or contains "@" */
             .replace(/^http:\/\/((?!ftp\d*\.)[^\/@]+(?:\/|$))/, "$1");
}
