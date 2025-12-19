// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         FinalizationRegistry Polyfill (best-effort)
// @namespace    internal-userscripts
// @description  Provides a minimal FinalizationRegistry stub for environments without native support. This cannot observe garbage collection; callbacks run asynchronously after register (best-effort stub).
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  function noop() {}

  // This implementation cannot receive GC notifications. As a best-effort
  // approximation it runs the cleanup callback asynchronously for every
  // registration unless the entry is unregistered first.
  function PolyfillFinalizationRegistry(cleanupCallback) {
    if (typeof cleanupCallback !== "function") {
      throw new TypeError("cleanup callback must be a function");
    }
    this._cleanupCallback = cleanupCallback;
    this._entries = new Map();
    this._pending = false;
  }

  PolyfillFinalizationRegistry.prototype.register = function (target, heldValue, unregisterToken) {
    if (target == null || (typeof target !== "object" && typeof target !== "function")) {
      throw new TypeError("register target must be an object");
    }
    // Store by token if provided; fall back to target identity.
    const key = unregisterToken || target;
    this._entries.set(key, heldValue);

    // Best-effort: fire cleanup almost immediately to mimic GC completion.
    if (!this._pending) {
      this._pending = true;
      setTimeout(() => {
        this._pending = false;
        this._runCleanup(this._cleanupCallback);
      }, 0);
    }
  };

  PolyfillFinalizationRegistry.prototype.unregister = function (unregisterToken) {
    if (unregisterToken == null) {
      return false;
    }
    return this._entries.delete(unregisterToken);
  };

  PolyfillFinalizationRegistry.prototype._runCleanup = function (cb) {
    cb = cb || noop;
    const entries = Array.from(this._entries.values());
    this._entries.clear();
    for (const held of entries) {
      try {
        cb(held);
      } catch (e) {
        // Swallow to continue other callbacks.
      }
    }
  };

  PolyfillFinalizationRegistry.prototype.cleanupSome = function (callback) {
    this._runCleanup(callback || this._cleanupCallback);
  };

  // Expose the stub globally (override to guarantee presence).
  // eslint-disable-next-line no-global-assign
  FinalizationRegistry = PolyfillFinalizationRegistry;
  try {
    // Marker for debugging/verification.
    window.__internalUserscriptsFinalizationRegistryPolyfill = true;
  } catch (e) {}
})();
