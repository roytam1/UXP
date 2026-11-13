// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         TextEncoderStream Polyfill (best-effort)
// @namespace    internal-userscripts
// @description  Minimal TextEncoderStream implementation backed by TransformStream.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  var global = typeof window !== "undefined" ? window : null;
  if (!global) {
    return;
  }

  var existingCtor = global.TextEncoderStream;
  if (typeof existingCtor === "function") {
    try {
      var probe = new existingCtor();
      if (probe && probe.readable && probe.writable) {
        return;
      }
    } catch (e) {
      if (String(e).indexOf("not yet implemented") === -1) {
        return;
      }
    }
  }

  if (typeof global.TransformStream !== "function") {
    return;
  }
  if (typeof global.TextEncoder !== "function") {
    return;
  }

  function isObject(value) {
    return value !== null && (typeof value === "object" || typeof value === "function");
  }

  function streamBrandCheckException(name) {
    return new TypeError("TextEncoderStream.prototype." + name +
      " can only be used on a TextEncoderStream");
  }

  function isTextEncoderStream(value) {
    return isObject(value) && value._isTextEncoderStream === true;
  }

  function PolyfillTextEncoderStream() {
    if (!(this instanceof PolyfillTextEncoderStream)) {
      throw new TypeError("TextEncoderStream must be constructed with 'new'");
    }

    var encoder = new global.TextEncoder();
    this._transform = new global.TransformStream({
      transform: function (chunk, controller) {
        controller.enqueue(encoder.encode(String(chunk)));
      }
    });
    this._readable = this._transform.readable;
    this._writable = this._transform.writable;
    this._isTextEncoderStream = true;
  }

  Object.defineProperty(PolyfillTextEncoderStream.prototype, "readable", {
    get: function () {
      if (!isTextEncoderStream(this)) {
        throw streamBrandCheckException("readable");
      }
      return this._readable;
    },
    enumerable: true,
    configurable: true
  });

  Object.defineProperty(PolyfillTextEncoderStream.prototype, "writable", {
    get: function () {
      if (!isTextEncoderStream(this)) {
        throw streamBrandCheckException("writable");
      }
      return this._writable;
    },
    enumerable: true,
    configurable: true
  });

  global.TextEncoderStream = PolyfillTextEncoderStream;
  try {
    global.__internalUserscriptsTextEncoderStreamPolyfill = true;
  } catch (e) {}
})();
