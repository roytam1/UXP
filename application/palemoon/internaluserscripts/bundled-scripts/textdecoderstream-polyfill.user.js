// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         TextDecoderStream Polyfill (best-effort)
// @namespace    internal-userscripts
// @description  Minimal TextDecoderStream implementation backed by TransformStream.
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

  var existingCtor = global.TextDecoderStream;
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
  if (typeof global.TextDecoder !== "function") {
    return;
  }

  function isObject(value) {
    return value !== null && (typeof value === "object" || typeof value === "function");
  }

  function streamBrandCheckException(name) {
    return new TypeError("TextDecoderStream.prototype." + name +
      " can only be used on a TextDecoderStream");
  }

  function isTextDecoderStream(value) {
    return isObject(value) && value._isTextDecoderStream === true;
  }

  function normalizeOptions(options) {
    var normalized = {
      fatal: false,
      ignoreBOM: false
    };

    if (!isObject(options)) {
      return normalized;
    }

    normalized.fatal = !!options.fatal;
    normalized.ignoreBOM = !!options.ignoreBOM;
    return normalized;
  }

  function PolyfillTextDecoderStream(label, options) {
    if (!(this instanceof PolyfillTextDecoderStream)) {
      throw new TypeError("TextDecoderStream must be constructed with 'new'");
    }

    var normalizedOptions = normalizeOptions(options);
    var effectiveLabel = label === undefined ? "utf-8" : label;

    var decoder;
    try {
      decoder = new global.TextDecoder(effectiveLabel, {
        fatal: normalizedOptions.fatal,
        ignoreBOM: normalizedOptions.ignoreBOM
      });
    } catch (e) {
      decoder = new global.TextDecoder(effectiveLabel, {
        fatal: normalizedOptions.fatal
      });
    }

    this._decoder = decoder;
    this._encoding = typeof decoder.encoding === "string"
      ? decoder.encoding
      : String(effectiveLabel).toLowerCase();
    this._fatal = typeof decoder.fatal === "boolean"
      ? decoder.fatal
      : normalizedOptions.fatal;
    this._ignoreBOM = typeof decoder.ignoreBOM === "boolean"
      ? decoder.ignoreBOM
      : normalizedOptions.ignoreBOM;

    this._transform = new global.TransformStream({
      transform: function (chunk, controller) {
        var output = decoder.decode(chunk, { stream: true });
        if (output.length > 0) {
          controller.enqueue(output);
        }
      },
      flush: function (controller) {
        var output = decoder.decode();
        if (output.length > 0) {
          controller.enqueue(output);
        }
      }
    });

    this._readable = this._transform.readable;
    this._writable = this._transform.writable;
    this._isTextDecoderStream = true;
  }

  Object.defineProperty(PolyfillTextDecoderStream.prototype, "encoding", {
    get: function () {
      if (!isTextDecoderStream(this)) {
        throw streamBrandCheckException("encoding");
      }
      return this._encoding;
    },
    enumerable: true,
    configurable: true
  });

  Object.defineProperty(PolyfillTextDecoderStream.prototype, "fatal", {
    get: function () {
      if (!isTextDecoderStream(this)) {
        throw streamBrandCheckException("fatal");
      }
      return this._fatal;
    },
    enumerable: true,
    configurable: true
  });

  Object.defineProperty(PolyfillTextDecoderStream.prototype, "ignoreBOM", {
    get: function () {
      if (!isTextDecoderStream(this)) {
        throw streamBrandCheckException("ignoreBOM");
      }
      return this._ignoreBOM;
    },
    enumerable: true,
    configurable: true
  });

  Object.defineProperty(PolyfillTextDecoderStream.prototype, "readable", {
    get: function () {
      if (!isTextDecoderStream(this)) {
        throw streamBrandCheckException("readable");
      }
      return this._readable;
    },
    enumerable: true,
    configurable: true
  });

  Object.defineProperty(PolyfillTextDecoderStream.prototype, "writable", {
    get: function () {
      if (!isTextDecoderStream(this)) {
        throw streamBrandCheckException("writable");
      }
      return this._writable;
    },
    enumerable: true,
    configurable: true
  });

  global.TextDecoderStream = PolyfillTextDecoderStream;
  try {
    global.__internalUserscriptsTextDecoderStreamPolyfill = true;
  } catch (e) {}
})();
