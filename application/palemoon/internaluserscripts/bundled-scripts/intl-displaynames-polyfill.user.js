// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         Intl.DisplayNames Polyfill (minimal)
// @namespace    internal-userscripts
// @description  Provides a minimal Intl.DisplayNames implementation for environments without native support.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  if (typeof Intl !== "object" || typeof Intl.DisplayNames === "function") {
    return;
  }

  var STYLE_VALUES = { long: true, short: true, narrow: true };
  var TYPE_VALUES = { language: true, region: true, script: true, currency: true };
  var FALLBACK_VALUES = { code: true, none: true };

  function canonicalizeLocales(locales) {
    if (typeof Intl.getCanonicalLocales === "function") {
      return Intl.getCanonicalLocales(locales);
    }
    if (locales == null) {
      return [];
    }
    if (Array.isArray(locales)) {
      return locales.map(function (loc) { return String(loc); });
    }
    return [String(locales)];
  }

  function defaultLocale() {
    try {
      if (typeof Intl.DateTimeFormat === "function") {
        return Intl.DateTimeFormat().resolvedOptions().locale;
      }
    } catch (e) {}
    if (typeof navigator !== "undefined" && navigator.language) {
      return String(navigator.language);
    }
    return "en-US";
  }

  function DisplayNames(locales, options) {
    if (!(this instanceof DisplayNames)) {
      throw new TypeError("Intl.DisplayNames must be called with new");
    }
    options = options || {};

    var type = options.type;
    if (type == null) {
      throw new TypeError("Intl.DisplayNames requires a type option");
    }
    type = String(type);
    if (!TYPE_VALUES[type]) {
      throw new RangeError("Invalid type");
    }

    var style = options.style == null ? "long" : String(options.style);
    if (!STYLE_VALUES[style]) {
      throw new RangeError("Invalid style");
    }

    var fallback = options.fallback == null ? "code" : String(options.fallback);
    if (!FALLBACK_VALUES[fallback]) {
      throw new RangeError("Invalid fallback");
    }

    var localeList = canonicalizeLocales(locales);
    this._locale = localeList.length ? localeList[0] : defaultLocale();
    this._style = style;
    this._type = type;
    this._fallback = fallback;
  }

  DisplayNames.supportedLocalesOf = function (locales) {
    return canonicalizeLocales(locales);
  };

  DisplayNames.prototype.resolvedOptions = function () {
    return {
      locale: this._locale,
      style: this._style,
      type: this._type,
      fallback: this._fallback,
    };
  };

  DisplayNames.prototype.of = function (code) {
    if (this._fallback === "none") {
      return undefined;
    }
    return String(code);
  };

  Intl.DisplayNames = DisplayNames;
  try {
    window.__internalUserscriptsIntlDisplayNamesPolyfill = true;
  } catch (e) {}
})();
