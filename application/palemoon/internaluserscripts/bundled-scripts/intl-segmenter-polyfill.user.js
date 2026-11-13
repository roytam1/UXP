// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         Intl.Segmenter Polyfill (minimal)
// @namespace    internal-userscripts
// @description  Provides a minimal Intl.Segmenter implementation for environments without native support.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  var global =
    (typeof globalThis !== "undefined" && globalThis) ||
    (typeof window !== "undefined" && window) ||
    (typeof self !== "undefined" && self) ||
    this;
  if (typeof global.Intl !== "object" || global.Intl == null) {
    global.Intl = {};
  }
  if (typeof global.Intl.Segmenter === "function") {
    return;
  }

  var GRANULARITY_VALUES = { grapheme: true, word: true, sentence: true };
  var LOCALE_MATCHER_VALUES = { lookup: true, "best fit": true };
  var FALLBACK_WORD_LIKE = /^[A-Za-z0-9_]+$/;
  var FALLBACK_WORD_SPLIT = /[A-Za-z0-9_]+|[\s]+|[^\s]/g;
  var COMBINING_MARK_RANGES = [
    [0x0300, 0x036f],
    [0x1ab0, 0x1aff],
    [0x1dc0, 0x1dff],
    [0x20d0, 0x20ff],
    [0xfe20, 0xfe2f],
  ];

  var unicodeGraphemeRegex = null;
  var unicodeWordSplit = null;
  var unicodeWordLike = null;
  try {
    unicodeGraphemeRegex = new RegExp(
      "(?:\\P{Mark}\\p{Mark}*|\\p{Mark}+)",
      "gu",
    );
    unicodeWordSplit = new RegExp("[\\p{L}\\p{N}\\p{M}_]+|[\\s]+|[^\\s]", "gu");
    unicodeWordLike = new RegExp("^[\\p{L}\\p{N}\\p{M}_]+$", "u");
  } catch (e) {}

  function canonicalizeLocales(locales) {
    if (typeof global.Intl.getCanonicalLocales === "function") {
      return global.Intl.getCanonicalLocales(locales);
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
      if (typeof global.Intl.DateTimeFormat === "function") {
        return global.Intl.DateTimeFormat().resolvedOptions().locale;
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

  function readCodePoint(str, index) {
    var first = str.charCodeAt(index);
    if (first >= 0xd800 && first <= 0xdbff && index + 1 < str.length) {
      var second = str.charCodeAt(index + 1);
      if (second >= 0xdc00 && second <= 0xdfff) {
        return {
          codePoint: ((first - 0xd800) << 10) + (second - 0xdc00) + 0x10000,
          length: 2,
        };
      }
    }
    return { codePoint: first, length: 1 };
  }

  function isCombiningMark(codePoint) {
    for (var i = 0; i < COMBINING_MARK_RANGES.length; i++) {
      var range = COMBINING_MARK_RANGES[i];
      if (codePoint >= range[0] && codePoint <= range[1]) {
        return true;
      }
    }
    return false;
  }

  function segmentGrapheme(string) {
    var segments = [];
    var i = 0;

    if (unicodeGraphemeRegex) {
      var match;
      unicodeGraphemeRegex.lastIndex = 0;
      while ((match = unicodeGraphemeRegex.exec(string))) {
        segments.push({
          segment: match[0],
          index: match.index,
        });
      }
      return segments;
    }

    while (i < string.length) {
      var start = i;
      var current = readCodePoint(string, i);
      i += current.length;
      while (i < string.length) {
        var next = readCodePoint(string, i);
        if (!isCombiningMark(next.codePoint)) {
          break;
        }
        i += next.length;
      }
      segments.push({
        segment: string.slice(start, i),
        index: start,
      });
    }

    return segments;
  }

  function segmentWord(string) {
    var segments = [];
    var regex = unicodeWordSplit || FALLBACK_WORD_SPLIT;
    var match;

    regex.lastIndex = 0;
    while ((match = regex.exec(string))) {
      segments.push({
        segment: match[0],
        index: match.index,
      });
    }
    return segments;
  }

  function isSentenceTerminal(ch) {
    return ch === "." || ch === "!" || ch === "?";
  }

  function isWhitespaceChar(ch) {
    return (
      ch === " " || ch === "\t" || ch === "\r" || ch === "\n" || ch === "\f"
    );
  }

  function segmentSentence(string) {
    var segments = [];
    var start = 0;
    var i = 0;

    if (!string) {
      return segments;
    }

    while (i < string.length) {
      if (isSentenceTerminal(string.charAt(i))) {
        i++;
        while (i < string.length && isSentenceTerminal(string.charAt(i))) {
          i++;
        }
        while (i < string.length && isWhitespaceChar(string.charAt(i))) {
          i++;
        }
        segments.push({
          segment: string.slice(start, i),
          index: start,
        });
        start = i;
      } else {
        i++;
      }
    }

    if (start < string.length) {
      segments.push({
        segment: string.slice(start),
        index: start,
      });
    }

    return segments;
  }

  function segmentString(string, granularity) {
    if (granularity === "word") {
      return segmentWord(string);
    }
    if (granularity === "sentence") {
      return segmentSentence(string);
    }
    return segmentGrapheme(string);
  }

  function isWordLikeSegment(segment) {
    if (unicodeWordLike) {
      return unicodeWordLike.test(segment);
    }
    return FALLBACK_WORD_LIKE.test(segment);
  }

  function requireSegmenter(value) {
    if (
      value == null ||
      (typeof value !== "object" && typeof value !== "function") ||
      !GRANULARITY_VALUES[value._granularity]
    ) {
      throw new TypeError(
        "Method Intl.Segmenter.prototype called on incompatible receiver",
      );
    }
    return value;
  }

  function SegmentIterator(segments) {
    this._segments = segments;
    this._index = 0;
  }

  SegmentIterator.prototype.next = function () {
    if (this._index >= this._segments.length) {
      return { value: undefined, done: true };
    }
    var value = this._segments[this._index];
    this._index += 1;
    return { value: value, done: false };
  };

  if (typeof Symbol !== "undefined" && Symbol.iterator) {
    SegmentIterator.prototype[Symbol.iterator] = function () {
      return this;
    };
  }

  function Segments(input, segments) {
    this._input = input;
    this._segments = segments;
  }

  Segments.prototype.containing = function (index) {
    if (index == null) {
      return undefined;
    }
    var position = Number(index);
    if (!isFinite(position)) {
      return undefined;
    }
    position = Math.floor(position);
    for (var i = 0; i < this._segments.length; i++) {
      var segment = this._segments[i];
      if (
        position >= segment.index &&
        position < segment.index + segment.segment.length
      ) {
        return segment;
      }
    }
    return undefined;
  };

  if (typeof Symbol !== "undefined" && Symbol.iterator) {
    Segments.prototype[Symbol.iterator] = function () {
      return new SegmentIterator(this._segments);
    };
  }

  function Segmenter(locales, options) {
    if (!(this instanceof Segmenter)) {
      throw new TypeError("Intl.Segmenter must be called with new");
    }

    if (options == null) {
      options = {};
    } else if (typeof options !== "object") {
      options = Object(options);
    }

    var granularity =
      options.granularity == null ? "grapheme" : String(options.granularity);
    if (!GRANULARITY_VALUES[granularity]) {
      throw new RangeError("Invalid granularity");
    }
    if (
      options.localeMatcher != null &&
      !LOCALE_MATCHER_VALUES[String(options.localeMatcher)]
    ) {
      throw new RangeError("Invalid localeMatcher");
    }

    var localeList = canonicalizeLocales(locales);
    this._locale = localeList.length ? localeList[0] : defaultLocale();
    this._granularity = granularity;
  }

  Segmenter.supportedLocalesOf = function (locales, options) {
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

  Segmenter.prototype.resolvedOptions = function () {
    var self = requireSegmenter(this);
    return {
      locale: self._locale,
      granularity: self._granularity,
    };
  };

  Segmenter.prototype.segment = function (input) {
    var self = requireSegmenter(this);
    var string = toStringValue(input);
    var entries = segmentString(string, self._granularity);
    var segments = [];

    for (var i = 0; i < entries.length; i++) {
      var part = entries[i];
      segments.push({
        segment: part.segment,
        index: part.index,
        input: string,
        isWordLike:
          self._granularity === "word"
            ? isWordLikeSegment(part.segment)
            : undefined,
      });
    }

    return new Segments(string, segments);
  };

  global.Intl.Segmenter = Segmenter;
  try {
    global.__internalUserscriptsIntlSegmenterPolyfill = true;
  } catch (e) {}
})();
