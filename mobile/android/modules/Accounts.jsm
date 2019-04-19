/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Accounts"];

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/Deprecated.jsm"); /*global Deprecated */
Cu.import("resource://gre/modules/Messaging.jsm"); /*global Messaging */
Cu.import("resource://gre/modules/Promise.jsm"); /*global Promise */
Cu.import("resource://gre/modules/Services.jsm"); /*global Services */

/**
 * A promise-based API for querying the existence of Sync accounts,
 * and accessing the Sync setup wizard.
 *
 * Usage:
 *
 *   Cu.import("resource://gre/modules/Accounts.jsm");
 *   Accounts.anySyncAccountsExist().then(
 *     (exist) => {
 *       console.log("Accounts exist? " + exist);
 *       if (!exist) {
 *         Accounts.launchSetup();
 *       }
 *     },
 *     (err) => {
 *       console.log("We failed so hard.");
 *     }
 *   );
 */
var Accounts = Object.freeze({
  _accountsExist: function (kind) {
    return Messaging.sendRequestForResult({
      type: "Accounts:Exist",
      kind: kind
    }).then(data => data.exists);
  },

  syncAccountsExist: function () {
    Deprecated.warning("The legacy Sync account type has been removed from Firefox for Android.");
    return Promise.resolve(false);
  },

  anySyncAccountsExist: function () {
    return this._accountsExist("any");
  },

  /**
   * Fire-and-forget: open the Firefox accounts activity, which
   * will be the Getting Started screen if FxA isn't yet set up.
   *
   * Optional extras are passed, as a JSON string, to the Firefox
   * Account Getting Started activity in the extras bundle of the
   * activity launch intent, under the key "extras".
   *
   * There is no return value from this method.
   */
  launchSetup: function (extras) {
    Messaging.sendRequest({
      type: "Accounts:Create",
      extras: extras
    });
  },

  _addDefaultEndpoints: function (json) {
    // Empty without FxA
    let newData = Cu.cloneInto(json, {}, { cloneFunctions: false });
    return newData;
  },

  showSyncPreferences: function () {
    // Only show Sync preferences of an existing Android Account.
    throw new Error("Can't show Sync preferences without accounts!");
  }
});
