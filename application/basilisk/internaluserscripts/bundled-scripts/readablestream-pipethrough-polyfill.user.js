// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         ReadableStream pipeThrough Polyfill (best-effort)
// @namespace    internal-userscripts
// @description  Adds ReadableStream.prototype.pipeThrough using pipeTo or reader/writer pumping when missing.
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
  var existingPipeThrough = global.ReadableStream.prototype.pipeThrough;
  if (typeof existingPipeThrough === "function") {
    if (typeof global.TransformStream !== "function") {
      return;
    }
    try {
      var probeStream = new global.ReadableStream({
        start: function (controller) {
          controller.close();
        }
      });
      probeStream.pipeThrough(new global.TransformStream(), { preventClose: true });
      return;
    } catch (e) {
      if (String(e).indexOf("not yet implemented") === -1) {
        return;
      }
    }
  }

  var PromiseCtor = global.Promise;
  if (typeof PromiseCtor !== "function") {
    return;
  }

  function isObject(value) {
    return value !== null && (typeof value === "object" || typeof value === "function");
  }

  function getTransformStream(transform, name) {
    var stream = transform[name];
    if (!isObject(stream)) {
      throw new TypeError("pipeThrough requires a transform stream with " + name);
    }
    return stream;
  }

  function startPipeWithReader(source, writable, options) {
    var reader = source.getReader();
    var writer = writable.getWriter();
    var preventClose = options && options.preventClose;
    var preventAbort = options && options.preventAbort;
    var preventCancel = options && options.preventCancel;
    var signal = options && options.signal;
    var abortHandler = null;

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

    if (signal) {
      abortHandler = function () {
        var reason = signal.reason;
        if (!preventCancel) {
          try { reader.cancel(reason); } catch (e) {}
        }
        if (!preventAbort) {
          try { writer.abort(reason); } catch (e) {}
        }
      };
      if (signal.aborted) {
        abortHandler();
        cleanupAbort();
        releaseLocks();
        return;
      }
      if (typeof signal.addEventListener === "function") {
        signal.addEventListener("abort", abortHandler);
      }
    }

    function pump() {
      var readPromise;
      try {
        readPromise = reader.read();
      } catch (e) {
        return PromiseCtor.reject(e);
      }
      return readPromise.then(function (result) {
        if (result.done) {
          if (!preventClose) {
            var closePromise;
            try {
              closePromise = writer.close();
            } catch (e) {
              return PromiseCtor.reject(e);
            }
            return PromiseCtor.resolve(closePromise);
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

    pump().catch(function (err) {
      if (!preventCancel) {
        try { reader.cancel(err); } catch (e) {}
      }
      if (!preventAbort) {
        try { writer.abort(err); } catch (e) {}
      }
    }).then(function () {
      cleanupAbort();
      releaseLocks();
    }, function () {
      cleanupAbort();
      releaseLocks();
    });
  }

  function pipeThrough(transform, options) {
    if (!isObject(this)) {
      throw new TypeError("ReadableStream.prototype.pipeThrough called on incompatible receiver");
    }
    if (!isObject(transform)) {
      throw new TypeError("pipeThrough requires a transform stream");
    }
    var readable = getTransformStream(transform, "readable");
    var writable = getTransformStream(transform, "writable");
    var source = this;

    if (typeof source.pipeTo === "function") {
      var pipePromise = source.pipeTo(writable, options);
      if (pipePromise && typeof pipePromise.catch === "function") {
        pipePromise.catch(function () {});
      }
    } else if (typeof source.getReader === "function" && typeof writable.getWriter === "function") {
      startPipeWithReader(source, writable, options);
    } else {
      throw new TypeError("ReadableStream.prototype.pipeThrough is not supported");
    }

    return readable;
  }

  Object.defineProperty(global.ReadableStream.prototype, "pipeThrough", {
    value: pipeThrough,
    writable: true,
    configurable: true
  });

  try {
    global.__internalUserscriptsReadableStreamPipeThroughPolyfill = true;
  } catch (e) {}
})();
