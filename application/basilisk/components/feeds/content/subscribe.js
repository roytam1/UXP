/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global BrowserFeedWriter */

var SubscribeHandler = {
  /**
   * The nsIFeedWriter object that produces the UI
   */
  _feedWriter: null,

  init: function() {
    this._feedWriter = new BrowserFeedWriter();
  },

  writeContent: function() {
    this._feedWriter.writeContent();
  },

  uninit: function() {
    this._feedWriter.close();
  }
};
