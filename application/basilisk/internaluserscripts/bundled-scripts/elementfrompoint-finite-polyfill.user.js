// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         elementFromPoint finite-args shim
// @namespace    internal-userscripts
// @description  Prevents throws when Document.elementFromPoint / elementsFromPoint receives non-finite coordinates.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  var global = typeof window !== "undefined" ? window : null;
  if (!global || !global.Document || !global.Document.prototype) {
    return;
  }

  var patched = false;

  function toFiniteNumber(value) {
    var number = Number(value);
    if (number !== number || number === Infinity || number === -Infinity) {
      return null;
    }
    return number;
  }

  function fallbackValue(methodName) {
    return methodName === "elementsFromPoint" ? [] : null;
  }

  function wrapMethod(proto, methodName) {
    var nativeMethod = proto[methodName];
    if (typeof nativeMethod !== "function") {
      return;
    }
    if (nativeMethod.__internalUserscriptsFinitePointShim) {
      return;
    }

    function wrappedMethod(x, y) {
      var finiteX = toFiniteNumber(x);
      var finiteY = toFiniteNumber(y);
      if (finiteX === null || finiteY === null) {
        return fallbackValue(methodName);
      }

      try {
        return nativeMethod.call(this, finiteX, finiteY);
      } catch (e) {
        var message = e && e.message ? String(e.message) : "";
        if (message.indexOf("not a finite floating-point value") !== -1 ||
            message.indexOf("is not finite") !== -1) {
          return fallbackValue(methodName);
        }
        throw e;
      }
    }

    try {
      Object.defineProperty(wrappedMethod, "__internalUserscriptsFinitePointShim", {
        value: true
      });
    } catch (e) {}

    try {
      Object.defineProperty(proto, methodName, {
        value: wrappedMethod,
        writable: true,
        configurable: true
      });
    } catch (e) {
      try {
        proto[methodName] = wrappedMethod;
      } catch (ignore) {}
    }

    if (proto[methodName] === wrappedMethod) {
      patched = true;
    }
  }

  wrapMethod(global.Document.prototype, "elementFromPoint");
  wrapMethod(global.Document.prototype, "elementsFromPoint");

  if (patched) {
    try {
      global.__internalUserscriptsElementFromPointFinitePolyfill = true;
    } catch (e) {}
  }
})();
