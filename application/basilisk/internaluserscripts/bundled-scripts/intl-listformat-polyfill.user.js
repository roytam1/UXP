// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         Intl.ListFormat Polyfill (minimal)
// @namespace    internal-userscripts
// @description  Provides a minimal Intl.ListFormat implementation for environments without native support.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  if (typeof Intl !== "object" || typeof Intl.ListFormat === "function") {
    return;
  }

  var TYPE_VALUES = { conjunction: true, disjunction: true, unit: true };
  var STYLE_VALUES = { long: true, short: true, narrow: true };
  var LOCALE_MATCHER_VALUES = { lookup: true, "best fit": true };
  var PATTERNS = {
    conjunction: {
      long: { pair: " and ", middle: ", ", end: ", and " },
      short: { pair: " & ", middle: ", ", end: ", & " },
      narrow: { pair: ", ", middle: ", ", end: ", " },
    },
    disjunction: {
      long: { pair: " or ", middle: ", ", end: ", or " },
      short: { pair: " or ", middle: ", ", end: ", or " },
      narrow: { pair: " or ", middle: ", ", end: ", or " },
    },
    unit: {
      long: { pair: ", ", middle: ", ", end: ", " },
      short: { pair: ", ", middle: ", ", end: ", " },
      narrow: { pair: " ", middle: " ", end: " " },
    },
  };

  function canonicalizeLocales(locales) {
    if (typeof Intl.getCanonicalLocales === "function") {
      return Intl.getCanonicalLocales(locales);
    }
    if (locales == null) {
      return [];
    }
    if (Array.isArray(locales)) {
      return locales.map(function (locale) {
        return String(locale);
      });
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

  function toStringValue(value) {
    if (typeof value === "symbol") {
      throw new TypeError("Cannot convert a Symbol value to a string");
    }
    return String(value);
  }

  function toStringList(list) {
    if (list == null) {
      throw new TypeError("List argument is required");
    }

    var result = [];
    if (typeof Symbol !== "undefined" && list[Symbol.iterator] != null) {
      var iterator = list[Symbol.iterator]();
      if (!iterator || typeof iterator.next !== "function") {
        throw new TypeError("List argument must be iterable");
      }
      while (true) {
        var step = iterator.next();
        if (step.done) {
          break;
        }
        result.push(toStringValue(step.value));
      }
      return result;
    }

    // Fallback for very old environments without Symbol.iterator.
    if (Array.isArray(list)) {
      for (var i = 0; i < list.length; i++) {
        result.push(toStringValue(list[i]));
      }
      return result;
    }

    throw new TypeError("List argument must be iterable");
  }

  function requireListFormatInstance(value) {
    if (
      value == null ||
      (typeof value !== "object" && typeof value !== "function") ||
      !TYPE_VALUES[value._type] ||
      !STYLE_VALUES[value._style]
    ) {
      throw new TypeError(
        "Method Intl.ListFormat.prototype called on incompatible receiver",
      );
    }
    return value;
  }

  function buildParts(items, pattern) {
    var parts = [];
    var i = 0;
    var lastIndex = items.length - 1;

    if (items.length === 0) {
      return parts;
    }
    if (items.length === 1) {
      parts.push({ type: "element", value: items[0] });
      return parts;
    }
    if (items.length === 2) {
      parts.push({ type: "element", value: items[0] });
      parts.push({ type: "literal", value: pattern.pair });
      parts.push({ type: "element", value: items[1] });
      return parts;
    }

    parts.push({ type: "element", value: items[0] });
    for (i = 1; i < lastIndex; i++) {
      parts.push({ type: "literal", value: pattern.middle });
      parts.push({ type: "element", value: items[i] });
    }
    parts.push({ type: "literal", value: pattern.end });
    parts.push({ type: "element", value: items[lastIndex] });
    return parts;
  }

  function ListFormat(locales, options) {
    if (!(this instanceof ListFormat)) {
      throw new TypeError("Intl.ListFormat must be called with new");
    }

    options = options || {};
    var type = options.type == null ? "conjunction" : String(options.type);
    var style = options.style == null ? "long" : String(options.style);
    var localeMatcher =
      options.localeMatcher == null
        ? "best fit"
        : String(options.localeMatcher);

    if (!TYPE_VALUES[type]) {
      throw new RangeError("Invalid type");
    }
    if (!STYLE_VALUES[style]) {
      throw new RangeError("Invalid style");
    }
    if (!LOCALE_MATCHER_VALUES[localeMatcher]) {
      throw new RangeError("Invalid localeMatcher");
    }

    var localeList = canonicalizeLocales(locales);
    this._locale = localeList.length ? localeList[0] : defaultLocale();
    this._type = type;
    this._style = style;
  }

  ListFormat.supportedLocalesOf = function (locales, options) {
    if (options != null && typeof options === "object") {
      var localeMatcher = options.localeMatcher;
      if (
        localeMatcher != null &&
        !LOCALE_MATCHER_VALUES[String(localeMatcher)]
      ) {
        throw new RangeError("Invalid localeMatcher");
      }
    }
    return canonicalizeLocales(locales);
  };

  ListFormat.prototype.resolvedOptions = function () {
    var self = requireListFormatInstance(this);
    return {
      locale: self._locale,
      type: self._type,
      style: self._style,
    };
  };

  ListFormat.prototype.formatToParts = function (list) {
    var self = requireListFormatInstance(this);
    var items = toStringList(list);
    var pattern = PATTERNS[self._type][self._style];
    return buildParts(items, pattern);
  };

  ListFormat.prototype._format = function (list) {
    var parts = this.formatToParts(list);
    var result = "";
    for (var i = 0; i < parts.length; i++) {
      result += parts[i].value;
    }
    return result;
  };

  Object.defineProperty(ListFormat.prototype, "format", {
    configurable: true,
    get: function () {
      var self = requireListFormatInstance(this);
      if (!self._boundFormat) {
        self._boundFormat = function (list) {
          return self._format(list);
        };
      }
      return self._boundFormat;
    },
  });

  Intl.ListFormat = ListFormat;
  try {
    window.__internalUserscriptsIntlListFormatPolyfill = true;
  } catch (e) {}
})();
