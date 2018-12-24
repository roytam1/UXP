/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "Deprecated" ];

const Cu = Components.utils;
const Ci = Components.interfaces;
const PREF_DEPRECATION_WARNINGS = "devtools.errorconsole.deprecation_warnings";
const PREF_PERFORMANCE_WARNINGS = "devtools.errorconsole.performance_warnings";

Cu.import("resource://gre/modules/Services.jsm");

// A flag that indicates whether deprecation warnings should be logged.
var logDepWarnings = Services.prefs.getBoolPref(PREF_DEPRECATION_WARNINGS);
var logPerfWarnings = Services.prefs.getBoolPref(PREF_PERFORMANCE_WARNINGS);

Services.prefs.addObserver(PREF_DEPRECATION_WARNINGS,
  function (aSubject, aTopic, aData) {
    logDepWarnings = Services.prefs.getBoolPref(PREF_DEPRECATION_WARNINGS);
  }, false);
Services.prefs.addObserver(PREF_PERFORMANCE_WARNINGS,
  function (aSubject, aTopic, aData) {
    logPerfWarnings = Services.prefs.getBoolPref(PREF_PERFORMANCE_WARNINGS);
  }, false);

/**
 * Build a callstack log message.
 *
 * @param nsIStackFrame aStack
 *        A callstack to be converted into a string log message.
 */
function stringifyCallstack (aStack) {
  // If aStack is invalid, use Components.stack (ignoring the last frame).
  if (!aStack || !(aStack instanceof Ci.nsIStackFrame)) {
    aStack = Components.stack.caller;
  }

  let frame = aStack.caller;
  let msg = "";
  // Get every frame in the callstack.
  while (frame) {
    msg += frame.filename + " " + frame.lineNumber +
      " " + frame.name + "\n";
    frame = frame.caller;
  }
  return msg;
}

this.Deprecated = {
  /**
   * Log a deprecation warning.
   *
   * @param string aText
   *        Deprecation warning text.
   * @param string aUrl
   *        A URL pointing to documentation describing deprecation
   *        and the way to address it.
   * @param nsIStackFrame aStack
   *        An optional callstack. If it is not provided a
   *        snapshot of the current JavaScript callstack will be
   *        logged.
   */
  warning: function (aText, aUrl, aStack) {
    if (!logDepWarnings) {
      return;
    }

    // If URL is not provided, report an error.
    if (!aUrl) {
      Cu.reportError("Error in Deprecated.warning: warnings must " +
        "provide a URL documenting this deprecation.");
      return;
    }

    let textMessage = "DEPRECATION WARNING: " + aText +
      "\nYou may find more details about this deprecation at: " +
      aUrl + "\nCallstack:\n" +
      // Append a callstack part to the deprecation message.
      stringifyCallstack(aStack);

    // Report deprecation warning.
    Cu.reportError(textMessage);
  },

  /**
   * Log a performance warning.
   *
   * @param string aText
   *        Performance issue warning text.
   * @param string aUrl
   *        A URL pointing to documentation describing performance
   *        issue and the way to address it.
   * @param nsIStackFrame aStack
   *        An optional callstack. If it is not provided a
   *        snapshot of the current JavaScript callstack will be
   *        logged.
   */
  perfWarning: function (aText, aUrl, aStack) {
    if (!logPerfWarnings) {
      return;
    }

    // If URL is not provided, report an error.
    if (!aUrl) {
      Cu.reportError("Error in Deprecated.perfWarning: warnings must " +
        "provide a URL documenting this performance issue.");
      return;
    }

    let textMessage = "PERFORMANCE WARNING: " + aText +
      "\nYou may find more details about this problem at: " +
      aUrl + "\nCallstack:\n" +
      // Append a callstack part to the deprecation message.
      stringifyCallstack(aStack);

    // Report deprecation warning.
    Cu.reportError(textMessage);
  }
};
