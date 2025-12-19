/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

if (typeof Cc === "undefined") {
  var Cc = Components.classes;
}
if (typeof Ci === "undefined") {
  var Ci = Components.interfaces;
}
if (typeof Cu === "undefined") {
  var Cu = Components.utils;
}

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

const PREF_ENABLED = "browser.internal-userscripts.enabled";

function InternalUserscriptsService() {
  this.wrappedJSObject = this;
}

InternalUserscriptsService.prototype = {
  classDescription: "Internal Userscripts Loader",
  classID: Components.ID("{77bf3650-1cd6-11da-8cd6-0800200c9a66}"),
  contractID: "@internaluserscripts.mozdev.org/service;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function(subject, topic, data) {
    if (topic === "profile-after-change") {
      this._startup();
    } else if (topic === "quit-application") {
      this._shutdown();
    } else if (topic === "document-element-inserted") {
      this._inject(subject && subject.defaultView);
    }
  },

  _startup: function() {
    if (!Services.prefs.getBoolPref(PREF_ENABLED, true)) {
      return;
    }

    Services.obs.addObserver(this, "document-element-inserted", false);
    Services.obs.addObserver(this, "quit-application", false);
  },

  _shutdown: function() {
    try { Services.obs.removeObserver(this, "document-element-inserted"); } catch (e) {}
    try { Services.obs.removeObserver(this, "quit-application"); } catch (e) {}
  },

  _inject: function(win) {
    if (!Services.prefs.getBoolPref(PREF_ENABLED, true)) {
      return;
    }
    if (!win) {
      return;
    }
    let contentWin = win.wrappedJSObject || win;

    try {
      Services.scriptloader.loadSubScript(
        "chrome://internaluserscripts/content/bundled-scripts/finalizationregistry-polyfill.user.js",
        contentWin
      );
    } catch (e) {
      // ignore; fallback below
    }

    try {
      const source = `
        (function(){
          var global = this;
          if (typeof global.FinalizationRegistry === "function" &&
              global.FinalizationRegistry.prototype &&
              typeof global.FinalizationRegistry.prototype.register === "function") {
            try { global.__internalUserscriptsFinalizationRegistryPolyfill = true; } catch (e) {}
            return;
          }
          function PolyfillFinalizationRegistry(cleanupCallback) {
            if (typeof cleanupCallback !== "function") {
              throw new TypeError("cleanup callback must be a function");
            }
            this._cleanupCallback = cleanupCallback;
            this._entries = [];
            this._pending = false;
          }
          PolyfillFinalizationRegistry.prototype.register = function (target, heldValue, unregisterToken) {
            if (target == null || (typeof target !== "object" && typeof target !== "function")) {
              throw new TypeError("register target must be an object");
            }
            var key = unregisterToken || target;
            this._entries.push({ key: key, held: heldValue });
            if (!this._pending) {
              this._pending = true;
              var self = this;
              global.setTimeout(function () {
                self._pending = false;
                self._runCleanup(self._cleanupCallback);
              }, 0);
            }
          };
          PolyfillFinalizationRegistry.prototype.unregister = function (unregisterToken) {
            if (unregisterToken == null) {
              return false;
            }
            var before = this._entries.length;
            this._entries = this._entries.filter(function (e) { return e.key !== unregisterToken; });
            return this._entries.length !== before;
          };
          PolyfillFinalizationRegistry.prototype._runCleanup = function (cb) {
            cb = cb || function () {};
            var entries = this._entries.slice(0);
            this._entries.length = 0;
            for (var i = 0; i < entries.length; i++) {
              try { cb(entries[i].held); } catch (e) {}
            }
          };
          PolyfillFinalizationRegistry.prototype.cleanupSome = function (callback) {
            this._runCleanup(callback || this._cleanupCallback);
          };
          global.FinalizationRegistry = PolyfillFinalizationRegistry;
          try { global.__internalUserscriptsFinalizationRegistryPolyfill = true; } catch (e) {}
        })();
      `;
      if (typeof contentWin.eval === "function") {
        contentWin.eval(source);
      }
    } catch (e) {
      // ignore
    }
  },
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([InternalUserscriptsService]);
