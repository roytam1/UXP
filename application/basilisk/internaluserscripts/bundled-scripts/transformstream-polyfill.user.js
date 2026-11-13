// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         TransformStream Polyfill (best-effort)
// @namespace    internal-userscripts
// @description  Minimal TransformStream implementation backed by ReadableStream and a lightweight WritableStream shim. Not a full Streams spec implementation.
// @match        *://*/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  var global = typeof window !== "undefined" ? window : null;
  if (!global || typeof global.TransformStream === "function") {
    return;
  }

  var ReadableStreamCtor = global.ReadableStream;
  var PromiseCtor = global.Promise;
  if (typeof PromiseCtor !== "function") {
    return;
  }

  function isObject(value) {
    return value !== null && (typeof value === "object" || typeof value === "function");
  }

  function streamBrandCheckException(name) {
    return new TypeError("TransformStream.prototype." + name +
      " can only be used on a TransformStream");
  }

  function PolyfillWritableStream(underlyingSink) {
    if (!(this instanceof PolyfillWritableStream)) {
      throw new TypeError("WritableStream must be constructed with 'new'");
    }
    this._sink = underlyingSink || {};
    this._state = "writable";
    this._storedError = undefined;
    this._writer = null;
    this._readyPromise = PromiseCtor.resolve();

    var resolveClosed;
    var rejectClosed;
    this._closedPromise = new PromiseCtor(function (resolve, reject) {
      resolveClosed = resolve;
      rejectClosed = reject;
    });
    this._closedResolve = resolveClosed;
    this._closedReject = rejectClosed;

    if (typeof this._sink.start === "function") {
      try {
        this._sink.start(this);
      } catch (e) {
        this._error(e);
      }
    }
  }

  PolyfillWritableStream.prototype.getWriter = function () {
    if (this._writer) {
      throw new TypeError("WritableStream is locked");
    }
    this._writer = new PolyfillWritableStreamDefaultWriter(this);
    return this._writer;
  };

  PolyfillWritableStream.prototype._releaseWriter = function (writer) {
    if (this._writer === writer) {
      this._writer = null;
    }
  };

  PolyfillWritableStream.prototype._error = function (reason) {
    if (this._state === "closed" || this._state === "errored") {
      return;
    }
    this._state = "errored";
    this._storedError = reason;
    if (this._closedReject) {
      this._closedReject(reason);
    }
  };

  PolyfillWritableStream.prototype._write = function (chunk) {
    if (this._state !== "writable") {
      return PromiseCtor.reject(new TypeError("WritableStream is not writable"));
    }
    if (!this._sink || typeof this._sink.write !== "function") {
      return PromiseCtor.resolve();
    }
    try {
      return PromiseCtor.resolve(this._sink.write(chunk));
    } catch (e) {
      this._error(e);
      return PromiseCtor.reject(e);
    }
  };

  PolyfillWritableStream.prototype._close = function () {
    if (this._state !== "writable") {
      return PromiseCtor.reject(new TypeError("WritableStream is not writable"));
    }
    this._state = "closing";

    var closeResult;
    if (this._sink && typeof this._sink.close === "function") {
      try {
        closeResult = this._sink.close();
      } catch (e) {
        this._error(e);
        return PromiseCtor.reject(e);
      }
    }

    var self = this;
    return PromiseCtor.resolve(closeResult).then(function () {
      self._state = "closed";
      if (self._closedResolve) {
        self._closedResolve();
      }
    }, function (e) {
      self._error(e);
      throw e;
    });
  };

  PolyfillWritableStream.prototype._abort = function (reason) {
    if (this._state === "closed") {
      return PromiseCtor.resolve();
    }
    this._error(reason);

    if (this._sink && typeof this._sink.abort === "function") {
      try {
        return PromiseCtor.resolve(this._sink.abort(reason));
      } catch (e) {
        return PromiseCtor.reject(e);
      }
    }
    return PromiseCtor.resolve();
  };

  function PolyfillWritableStreamDefaultWriter(stream) {
    this._stream = stream;
  }

  PolyfillWritableStreamDefaultWriter.prototype._assertStream = function (name) {
    if (!this._stream) {
      throw new TypeError("Cannot " + name + " a stream using a released writer");
    }
    return this._stream;
  };

  Object.defineProperty(PolyfillWritableStreamDefaultWriter.prototype, "ready", {
    get: function () {
      return this._assertStream("get ready state for")._readyPromise;
    },
    enumerable: true,
    configurable: true
  });

  Object.defineProperty(PolyfillWritableStreamDefaultWriter.prototype, "closed", {
    get: function () {
      return this._assertStream("get closed state for")._closedPromise;
    },
    enumerable: true,
    configurable: true
  });

  PolyfillWritableStreamDefaultWriter.prototype.write = function (chunk) {
    return this._assertStream("write to")._write(chunk);
  };

  PolyfillWritableStreamDefaultWriter.prototype.close = function () {
    return this._assertStream("close")._close();
  };

  PolyfillWritableStreamDefaultWriter.prototype.abort = function (reason) {
    return this._assertStream("abort")._abort(reason);
  };

  PolyfillWritableStreamDefaultWriter.prototype.releaseLock = function () {
    var stream = this._stream;
    if (!stream) {
      return;
    }
    stream._releaseWriter(this);
    this._stream = null;
  };

  var WritableStreamCtor = typeof global.WritableStream === "function"
    ? global.WritableStream
    : PolyfillWritableStream;

  function TransformStreamDefaultController(stream) {
    this._stream = stream;
    this._readableController = null;
  }

  Object.defineProperty(TransformStreamDefaultController.prototype, "desiredSize", {
    get: function () {
      if (!this._readableController) {
        return null;
      }
      return this._readableController.desiredSize;
    },
    enumerable: true,
    configurable: true
  });

  TransformStreamDefaultController.prototype.enqueue = function (chunk) {
    var controller = this._readableController;
    if (!controller) {
      throw new TypeError("ReadableStream controller is not available");
    }
    controller.enqueue(chunk);
  };

  TransformStreamDefaultController.prototype.error = function (reason) {
    this._stream._error(reason);
  };

  TransformStreamDefaultController.prototype.terminate = function () {
    this._stream._terminate();
  };

  function isTransformStream(value) {
    return isObject(value) && value._isTransformStream === true;
  }

  function PolyfillTransformStream(transformer, writableStrategy, readableStrategy) {
    if (!(this instanceof PolyfillTransformStream)) {
      throw new TypeError("TransformStream must be constructed with 'new'");
    }

    if (typeof ReadableStreamCtor !== "function") {
      throw new TypeError("ReadableStream is not supported in this environment");
    }

    transformer = transformer || {};
    writableStrategy = writableStrategy || {};
    readableStrategy = readableStrategy || {};

    if (transformer.writableType !== undefined) {
      throw new RangeError("Invalid writable type specified");
    }
    if (transformer.readableType !== undefined) {
      throw new RangeError("Invalid readable type specified");
    }

    this._transformer = transformer;
    this._controller = new TransformStreamDefaultController(this);
    this._readableClosed = false;
    this._errored = false;
    this._terminated = false;
    this._pendingError = undefined;
    this._hasPendingError = false;
    this._isTransformStream = true;

    var self = this;

    this._readable = new ReadableStreamCtor({
      start: function (controller) {
        self._controller._readableController = controller;
        if (self._hasPendingError) {
          controller.error(self._pendingError);
          return;
        }
        if (typeof transformer.start === "function") {
          return transformer.start(self._controller);
        }
      },
      pull: function () {},
      cancel: function (reason) {
        self._error(reason);
        return PromiseCtor.resolve();
      }
    }, readableStrategy);

    this._writable = new WritableStreamCtor({
      write: function (chunk) {
        return self._transform(chunk);
      },
      close: function () {
        return self._flush();
      },
      abort: function (reason) {
        self._error(reason);
        return PromiseCtor.resolve();
      }
    }, writableStrategy);
  }

  Object.defineProperty(PolyfillTransformStream.prototype, "readable", {
    get: function () {
      if (!isTransformStream(this)) {
        throw streamBrandCheckException("readable");
      }
      return this._readable;
    },
    enumerable: true,
    configurable: true
  });

  Object.defineProperty(PolyfillTransformStream.prototype, "writable", {
    get: function () {
      if (!isTransformStream(this)) {
        throw streamBrandCheckException("writable");
      }
      return this._writable;
    },
    enumerable: true,
    configurable: true
  });

  PolyfillTransformStream.prototype._transform = function (chunk) {
    if (this._errored || this._terminated) {
      return PromiseCtor.reject(new TypeError("TransformStream is not writable"));
    }

    var transformer = this._transformer;
    var controller = this._controller;
    var result;

    try {
      if (typeof transformer.transform === "function") {
        result = transformer.transform(chunk, controller);
      } else {
        controller.enqueue(chunk);
      }
    } catch (e) {
      this._error(e);
      return PromiseCtor.reject(e);
    }

    var self = this;
    return PromiseCtor.resolve(result).then(function () {
      return undefined;
    }, function (e) {
      self._error(e);
      throw e;
    });
  };

  PolyfillTransformStream.prototype._flush = function () {
    if (this._errored || this._terminated) {
      return PromiseCtor.reject(new TypeError("TransformStream is not writable"));
    }

    var transformer = this._transformer;
    var controller = this._controller;
    var result;

    try {
      if (typeof transformer.flush === "function") {
        result = transformer.flush(controller);
      }
    } catch (e) {
      this._error(e);
      return PromiseCtor.reject(e);
    }

    var self = this;
    return PromiseCtor.resolve(result).then(function () {
      self._closeReadable();
    }, function (e) {
      self._error(e);
      throw e;
    });
  };

  PolyfillTransformStream.prototype._closeReadable = function () {
    if (this._readableClosed) {
      return;
    }
    this._readableClosed = true;

    var controller = this._controller._readableController;
    if (!controller) {
      return;
    }
    try {
      controller.close();
    } catch (e) {}
  };

  PolyfillTransformStream.prototype._error = function (reason) {
    if (this._errored) {
      return;
    }
    this._errored = true;

    var controller = this._controller._readableController;
    if (controller) {
      try {
        controller.error(reason);
      } catch (e) {}
    } else {
      this._pendingError = reason;
      this._hasPendingError = true;
    }

    if (this._writable && typeof this._writable._error === "function") {
      this._writable._error(reason);
    }
  };

  PolyfillTransformStream.prototype._terminate = function () {
    if (this._terminated) {
      return;
    }
    this._terminated = true;
    this._closeReadable();
    if (this._writable && typeof this._writable._error === "function") {
      this._writable._error(new TypeError("TransformStream terminated"));
    }
  };

  global.TransformStream = PolyfillTransformStream;
  if (typeof global.WritableStream !== "function") {
    global.WritableStream = PolyfillWritableStream;
  }
  try {
    global.__internalUserscriptsTransformStreamPolyfill = true;
  } catch (e) {}
})();
