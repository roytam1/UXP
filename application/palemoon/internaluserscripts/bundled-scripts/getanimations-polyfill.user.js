// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         getAnimations Polyfill (minimal)
// @namespace    internal-userscripts
// @description  Adds minimal Document/Element getAnimations() methods that return an empty array when unsupported.
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

  var patched = false;

  function getAnimations() {
    return [];
  }

  function defineGetAnimations(proto) {
    if (!proto || typeof proto.getAnimations === "function") {
      return;
    }

    try {
      Object.defineProperty(proto, "getAnimations", {
        value: getAnimations,
        writable: true,
        configurable: true
      });
    } catch (e) {
      proto.getAnimations = getAnimations;
    }
    patched = true;
  }

  defineGetAnimations(global.Document && global.Document.prototype);
  defineGetAnimations(global.Element && global.Element.prototype);
  defineGetAnimations(global.CSSPseudoElement && global.CSSPseudoElement.prototype);

  if (patched) {
    try {
      global.__internalUserscriptsGetAnimationsPolyfill = true;
    } catch (e) {}
  }
})();
