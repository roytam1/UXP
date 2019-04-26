// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file and Readability-readerable.js are merged together into
// Readerable.jsm.

/* exported Readerable */
/* import-globals-from Readability-readerable.js */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function isNodeVisible(node) {
  return node.clientHeight > 0 && node.clientWidth > 0;
}

var Readerable = {
  DEBUG: 0,

  get isEnabledForParseOnLoad() {
    delete this.isEnabledForParseOnLoad;

    // Listen for future pref changes.
    Services.prefs.addObserver("reader.parse-on-load.", this, false);

    return this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
  },

  _getStateForParseOnLoad() {
    let isEnabled = Services.prefs.getBoolPref("reader.parse-on-load.enabled");
    let isForceEnabled = Services.prefs.getBoolPref("reader.parse-on-load.force-enabled");
    return isForceEnabled || isEnabled;
  },

  observe(aMessage, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        if (aData.startsWith("reader.parse-on-load.")) {
          this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
        } else if (aData === "reader.parse-node-limit") {
          this.parseNodeLimit = Services.prefs.getIntPref(aData);
        }
        break;
    }
  },

  log(msg) {
    if (this.DEBUG)
      dump("Reader: " + msg);
  },

  /**
   * Decides whether or not a document is reader-able without parsing the whole thing.
   *
   * @param doc A document to parse.
   * @return boolean Whether or not we should show the reader mode button.
   */
  isProbablyReaderable(doc) {
    // Only care about 'real' HTML documents:
    if (doc.mozSyntheticDocument || !(doc instanceof doc.defaultView.HTMLDocument)) {
      return false;
    }

    let uri = Services.io.newURI(doc.location.href);
    if (!this.shouldCheckUri(uri)) {
      return false;
    }

    return isProbablyReaderable(doc, isNodeVisible);
  },

  _blockedHosts: [
    "amazon.com",
    "basilisk-browser.org",
    "github.com",
    "mail.google.com",
    "palemoon.org",
    "pinterest.com",
    "reddit.com",
    "twitter.com",
    "youtube.com",
  ],

  shouldCheckUri(uri, isBaseUri = false) {
    if (!(uri.schemeIs("http") || uri.schemeIs("https"))) {
      this.log("Not parsing URI scheme: " + uri.scheme);
      return false;
    }

    try {
      uri.QueryInterface(Ci.nsIURL);
    } catch (ex) {
      // If this doesn't work, presumably the URL is not well-formed or something
      return false;
    }
    // Sadly, some high-profile pages have false positives, so bail early for those:
    let asciiHost = uri.asciiHost;
    if (!isBaseUri && this._blockedHosts.some(blockedHost => asciiHost.endsWith(blockedHost))) {
      return false;
    }

    if (!isBaseUri && (!uri.filePath || uri.filePath == "/")) {
      this.log("Not parsing home page: " + uri.spec);
      return false;
    }

    return true;
  },
};
