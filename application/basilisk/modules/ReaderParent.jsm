// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ "ReaderParent" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils", "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");

const gStringBundle = Services.strings.createBundle("chrome://global/locale/aboutReader.properties");

var ReaderParent = {
  MESSAGES: [
    "Reader:UpdateReaderButton",
  ],

  init() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }
  },

  receiveMessage(message) {
    switch (message.name) {
      case "Reader:UpdateReaderButton": {
        let browser = message.target;
        if (message.data && message.data.isArticle !== undefined) {
          browser.isArticle = message.data.isArticle;
        }
        this.updateReaderButton(browser);
        break;
      }
    }
  },

  updateReaderButton(browser) {
    let win = browser.ownerGlobal;
    if (browser != win.gBrowser.selectedBrowser) {
      return;
    }

    let button = win.document.getElementById("reader-mode-button");
    let command = win.document.getElementById("View:ReaderView");
    let key = win.document.getElementById("key_toggleReaderMode");
    // aria-reader is not a real ARIA attribute. However, this will cause
    // Gecko accessibility to expose the "reader" object attribute. We do this
    // so that the reader state is easy for accessibility clients to access
    // programmatically.
    if (browser.currentURI.spec.startsWith("about:reader")) {
      button.setAttribute("readeractive", true);
      button.hidden = false;
      let closeText = gStringBundle.GetStringFromName("readerView.close");
      button.setAttribute("tooltiptext", closeText);
      command.setAttribute("label", closeText);
      command.setAttribute("hidden", false);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.close.accesskey"));
      key.setAttribute("disabled", false);
      browser.setAttribute("aria-reader", "active");
    } else {
      button.removeAttribute("readeractive");
      button.hidden = !browser.isArticle;
      let enterText = gStringBundle.GetStringFromName("readerView.enter");
      button.setAttribute("tooltiptext", enterText);
      command.setAttribute("label", enterText);
      command.setAttribute("hidden", !browser.isArticle);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.enter.accesskey"));
      key.setAttribute("disabled", !browser.isArticle);
      if (browser.isArticle) {
        browser.setAttribute("aria-reader", "available");
      } else {
        browser.removeAttribute("aria-reader");
      }
    }
  },

  forceShowReaderIcon(browser) {
    browser.isArticle = true;
    this.updateReaderButton(browser);
  },

  buttonClick(event) {
    if (event.button != 0) {
      return;
    }
    this.toggleReaderMode(event);
  },

  toggleReaderMode(event) {
    let win = event.target.ownerGlobal;
    let browser = win.gBrowser.selectedBrowser;
    browser.messageManager.sendAsyncMessage("Reader:ToggleReaderMode");
  }
};
