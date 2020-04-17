/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// See: netwerk/protocol/about/nsIAboutModule.idl
const URI_SAFE_FOR_UNTRUSTED_CONTENT  = Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT;
const ALLOW_SCRIPT                    = Ci.nsIAboutModule.ALLOW_SCRIPT;
const HIDE_FROM_ABOUTABOUT            = Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT;
const MAKE_LINKABLE                   = Ci.nsIAboutModule.MAKE_LINKABLE;

function AboutRedirector() {}
AboutRedirector.prototype = {
  classDescription: "Browser about: Redirector",
  classID: Components.ID("{8cc51368-6aa0-43e8-b762-bde9b9fd828c}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),

  // Each entry in the map has the key as the part after the "about:" and the
  // value as a record with url and flags entries. Note that each addition here
  // should be coupled with a corresponding addition in BrowserComponents.manifest.
  _redirMap: {
    "certerror": {
      url: "chrome://browser/content/certerror/aboutCertError.xhtml",
      flags: (URI_SAFE_FOR_UNTRUSTED_CONTENT | ALLOW_SCRIPT | HIDE_FROM_ABOUTABOUT)
    },
    "downloads": {
      url: "chrome://browser/content/downloads/contentAreaDownloadsView.xul",
      flags: ALLOW_SCRIPT
    },
    "feeds": {
      url: "chrome://browser/content/feeds/subscribe.xhtml",
      flags: (URI_SAFE_FOR_UNTRUSTED_CONTENT | ALLOW_SCRIPT | HIDE_FROM_ABOUTABOUT)
    },
    "home": {
      url: "chrome://browser/content/abouthome/aboutHome.xhtml",
      flags: (URI_SAFE_FOR_UNTRUSTED_CONTENT | MAKE_LINKABLE | ALLOW_SCRIPT)
    },
    "newtab": {
      url: "chrome://browser/content/newtab/newTab.xhtml",
      flags: ALLOW_SCRIPT
    },
    "palemoon": {
      url: "chrome://browser/content/palemoon.xhtml",
      flags: (URI_SAFE_FOR_UNTRUSTED_CONTENT | HIDE_FROM_ABOUTABOUT)
    },
    "permissions": {
      url: "chrome://browser/content/permissions/aboutPermissions.xul",
      flags: ALLOW_SCRIPT
    },
    "privatebrowsing": {
      url: "chrome://browser/content/aboutPrivateBrowsing.xhtml",
      flags: ALLOW_SCRIPT
    },
    "rights": {
      url: "chrome://global/content/aboutRights.xhtml",
      flags: (URI_SAFE_FOR_UNTRUSTED_CONTENT | MAKE_LINKABLE | ALLOW_SCRIPT)
    },
    "sessionrestore": {
      url: "chrome://browser/content/aboutSessionRestore.xhtml",
      flags: ALLOW_SCRIPT
    },
#ifdef MOZ_SERVICES_SYNC
    "sync-progress": {
      url: "chrome://browser/content/sync/progress.xhtml",
      flags: ALLOW_SCRIPT
    },
    "sync-tabs": {
      url: "chrome://browser/content/sync/aboutSyncTabs.xul",
      flags: ALLOW_SCRIPT
    },
#endif
  },

  /**
   * Gets the module name from the given URI.
   */
  _getModuleName: function(aURI) {
    // Strip out the first ? or #, and anything following it
    let name = (/[^?#]+/.exec(aURI.path))[0];
    return name.toLowerCase();
  },

  getURIFlags: function(aURI) {
    let name = this._getModuleName(aURI);
    if (!(name in this._redirMap)) {
      throw Cr.NS_ERROR_ILLEGAL_VALUE;
    }
    return this._redirMap[name].flags;
  },

  newChannel: function(aURI, aLoadInfo) {
    let name = this._getModuleName(aURI);
    if (!(name in this._redirMap)) {
      throw Cr.NS_ERROR_ILLEGAL_VALUE;
    }

    let newURI = Services.io.newURI(this._redirMap[name].url, null, null);
    let channel = Services.io.newChannelFromURIWithLoadInfo(newURI, aLoadInfo);
    channel.originalURI = aURI;

    if (this._redirMap[name].flags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) {
      let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(aURI);
      channel.owner = principal;
    }

    return channel;
  }
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([AboutRedirector]);
