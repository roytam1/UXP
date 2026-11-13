// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         ReadableStream pipeTo Polyfill (best-effort)
// @namespace    internal-userscripts
// @description  Adds ReadableStream.prototype.pipeTo using reader/writer pumping.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  var global = typeof window !== "undefined" ? window : null;
  if (!global || !global.ReadableStream || !global.ReadableStream.prototype) {
    return;
  }

  var PromiseCtor = global.Promise;
  if (typeof PromiseCtor !== "function") {
    return;
  }

  function isObject(value) {
    return value !== null && (typeof value === "object" || typeof value === "function");
  }

  var existingPipeTo = global.ReadableStream.prototype.pipeTo;
  if (typeof existingPipeTo === "function") {
    if (typeof global.TransformStream !== "function" &&
        typeof global.WritableStream !== "function") {
      return;
    }
    try {
      var testWritable = null;
      if (typeof global.TransformStream === "function") {
        testWritable = new global.TransformStream().writable;
      } else {
        testWritable = new global.WritableStream();
      }
      var probeStream = new global.ReadableStream({
        start: function (controller) {
          controller.close();
        }
      });
      probeStream.pipeTo(testWritable, { preventClose: true });
      return;
    } catch (e) {
      if (String(e).indexOf("not yet implemented") === -1) {
        return;
      }
    }
  }

  function pipeTo(writable, options) {
    if (!isObject(this)) {
      throw new TypeError("ReadableStream.prototype.pipeTo called on incompatible receiver");
    }
    if (!isObject(writable) || typeof writable.getWriter !== "function") {
      throw new TypeError("pipeTo requires a writable stream");
    }

    var source = this;
    var reader = source.getReader();
    var writer = writable.getWriter();
    var preventClose = options && options.preventClose;
    var preventAbort = options && options.preventAbort;
    var preventCancel = options && options.preventCancel;
    var signal = options && options.signal;
    var abortHandler = null;
    var abortReject = null;
    var aborted = false;
    var abortReason = undefined;

    function safeReleaseLock(stream, maybeReader, lockedProp) {
      if (!maybeReader) {
        return;
      }
      var isLocked = true;
      if (typeof lockedProp === "function") {
        try {
          var lockedValue = lockedProp();
          if (typeof lockedValue === "boolean") {
            isLocked = lockedValue;
          }
        } catch (e) {
          isLocked = true;
        }
      }
      if (!isLocked) {
        return;
      }
      var releaseFn = null;
      try {
        releaseFn = maybeReader.releaseLock;
      } catch (e) {
        releaseFn = null;
      }
      if (typeof releaseFn === "function") {
        try { releaseFn.call(maybeReader); } catch (e) {}
      }
    }

    function releaseLocks() {
      safeReleaseLock(source, reader, function () { return source.locked; });
      safeReleaseLock(writable, writer, function () { return writable.locked; });
      reader = null;
      writer = null;
    }

    function cleanupAbort() {
      if (abortHandler && signal && typeof signal.removeEventListener === "function") {
        signal.removeEventListener("abort", abortHandler);
      }
    }

    function abortWith(reason) {
      if (aborted) {
        return;
      }
      aborted = true;
      abortReason = reason;
      if (!preventCancel) {
        try { reader.cancel(reason); } catch (e) {}
      }
      if (!preventAbort) {
        try { writer.abort(reason); } catch (e) {}
      }
      if (abortReject) {
        abortReject(reason);
      }
    }

    var abortPromise = null;
    if (signal) {
      abortPromise = new PromiseCtor(function (_resolve, reject) {
        abortReject = reject;
      });
      abortHandler = function () {
        abortWith(signal.reason);
      };
      if (signal.aborted) {
        abortWith(signal.reason);
      } else if (typeof signal.addEventListener === "function") {
        signal.addEventListener("abort", abortHandler);
      }
    }

    function pump() {
      if (aborted) {
        return PromiseCtor.reject(abortReason);
      }
      var readPromise;
      try {
        readPromise = reader.read();
      } catch (e) {
        return PromiseCtor.reject(e);
      }
      return readPromise.then(function (result) {
        if (aborted) {
          throw abortReason;
        }
        if (result.done) {
          if (!preventClose) {
            return writer.close();
          }
          return undefined;
        }
        var writePromise;
        try {
          writePromise = writer.write(result.value);
        } catch (e) {
          return PromiseCtor.reject(e);
        }
        return PromiseCtor.resolve(writePromise).then(pump);
      });
    }

    var pipePromise = pump().catch(function (err) {
      if (!preventCancel) {
        try { reader.cancel(err); } catch (e) {}
      }
      if (!preventAbort) {
        try { writer.abort(err); } catch (e) {}
      }
      throw err;
    });

    var finalPromise = abortPromise
      ? PromiseCtor.race([pipePromise, abortPromise])
      : pipePromise;

    function settleAndRelease(err) {
      return pipePromise.catch(function () {}).then(function () {
        cleanupAbort();
        releaseLocks();
        if (err !== undefined) {
          throw err;
        }
        return undefined;
      });
    }

    return finalPromise.then(function () {
      return settleAndRelease();
    }, function (err) {
      return settleAndRelease(err);
    });
  }

  Object.defineProperty(global.ReadableStream.prototype, "pipeTo", {
    value: pipeTo,
    writable: true,
    configurable: true
  });

  try {
    global.__internalUserscriptsReadableStreamPipeToPolyfill = true;
  } catch (e) {}
})();
