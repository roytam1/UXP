// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         HTMLImageElement.decode Polyfill (best-effort)
// @namespace    internal-userscripts
// @description  Adds a Promise-based decode() that resolves on load and rejects on error.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  var global = typeof window !== "undefined" ? window : null;
  if (!global || !global.HTMLImageElement) {
    return;
  }

  var proto = global.HTMLImageElement.prototype;
  if (typeof proto.decode === "function") {
    return;
  }

  var PromiseCtor = global.Promise;
  if (typeof PromiseCtor !== "function") {
    return;
  }

  function rejectPromise(error) {
    return new PromiseCtor(function (resolve, reject) {
      reject(error);
    });
  }

  function hasLoadedImage(img) {
    return img.complete && (img.naturalWidth > 0 || img.naturalHeight > 0);
  }

  function isBrokenImage(img) {
    return img.complete && img.naturalWidth === 0 && img.naturalHeight === 0;
  }

  function decode() {
    var img = this;
    if (!(img instanceof global.HTMLImageElement)) {
      return rejectPromise(new TypeError("decode can only be used on an HTMLImageElement"));
    }

    if (hasLoadedImage(img)) {
      return PromiseCtor.resolve();
    }
    if (isBrokenImage(img)) {
      return rejectPromise(new Error("Image decode failed"));
    }

    var src = "";
    try {
      src = img.currentSrc || img.src || "";
    } catch (e) {}
    if (!src) {
      return rejectPromise(new Error("Image has no src"));
    }

    return new PromiseCtor(function (resolve, reject) {
      var settled = false;

      function cleanup() {
        if (settled) {
          return;
        }
        settled = true;
        img.removeEventListener("load", onLoad, false);
        img.removeEventListener("error", onError, false);
      }

      function onLoad() {
        cleanup();
        resolve();
      }

      function onError() {
        cleanup();
        reject(new Error("Image decode failed"));
      }

      img.addEventListener("load", onLoad, false);
      img.addEventListener("error", onError, false);

      if (hasLoadedImage(img)) {
        cleanup();
        resolve();
      } else if (isBrokenImage(img)) {
        cleanup();
        reject(new Error("Image decode failed"));
      }
    });
  }

  try {
    Object.defineProperty(proto, "decode", {
      value: decode,
      writable: true,
      configurable: true
    });
  } catch (e) {
    proto.decode = decode;
  }

  try { global.__internalUserscriptsImageDecodePolyfill = true; } catch (e) {}
})();
