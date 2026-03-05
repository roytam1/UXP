/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

assertEq(typeof Array.prototype.toReversed, "function");
assertEq(Array.prototype.toReversed.length, 0);

let desc = Object.getOwnPropertyDescriptor(Array.prototype, "toReversed");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertThrowsInstanceOf(function () {
  Array.prototype.toReversed.call(null);
}, TypeError);
assertThrowsInstanceOf(function () {
  Array.prototype.toReversed.call(undefined);
}, TypeError);

// Non-mutating behavior.
let original = [1, 2, 3];
let reversed = original.toReversed();
assertEq(original !== reversed, true);
assertEq(original.join(","), "1,2,3");
assertEq(reversed.join(","), "3,2,1");

// Holes are treated as undefined and become actual properties on the result.
let sparse = [1, , 3];
let sparseReversed = sparse.toReversed();
assertEq(sparseReversed.length, 3);
assertEq(sparseReversed[0], 3);
assertEq(sparseReversed[1], undefined);
assertEq(sparseReversed[2], 1);
assertEq(1 in sparseReversed, true);

// Generic behavior on array-like values.
let arrayLike = { 0: "a", 2: "c", length: 3 };
let arrayLikeReversed = Array.prototype.toReversed.call(arrayLike);
assertEq(Array.isArray(arrayLikeReversed), true);
assertEq(arrayLikeReversed.length, 3);
assertEq(arrayLikeReversed[0], "c");
assertEq(arrayLikeReversed[1], undefined);
assertEq(arrayLikeReversed[2], "a");
assertEq(1 in arrayLikeReversed, true);

// Source element access is in descending index order.
let accessLog = [];
let getterArr = {
  length: 3,
  get 0() {
    accessLog.push(0);
    return "zero";
  },
  get 1() {
    accessLog.push(1);
    return "one";
  },
  get 2() {
    accessLog.push(2);
    return "two";
  },
};
let getterReversed = Array.prototype.toReversed.call(getterArr);
assertEq(accessLog.join(","), "2,1,0");
assertEq(getterReversed.join(","), "two,one,zero");

// toReversed ignores Array @@species and always creates a plain Array.
let speciesLog = [];
let speciesArr = [10, 20];
Object.defineProperty(speciesArr, "constructor", {
  get: function () {
    speciesLog.push("constructor");
    return {
      get [Symbol.species]() {
        speciesLog.push("species");
        return function () {};
      },
    };
  },
  configurable: true,
});
let speciesResult = speciesArr.toReversed();
assertEq(speciesLog.join(","), "");
assertEq(Array.isArray(speciesResult), true);
assertEq(speciesResult.join(","), "20,10");

// toReversed must not read "constructor" at all.
let throwLog = [];
let throwArr = [1];
Object.defineProperty(throwArr, "0", {
  get: function () {
    throwLog.push("get0");
    return 1;
  },
  configurable: true,
});
Object.defineProperty(throwArr, "constructor", {
  get: function () {
    throwLog.push("constructor");
    throw new RangeError("boom");
  },
  configurable: true,
});
let throwResult = throwArr.toReversed();
assertEq(throwLog.join(","), "get0");
assertEq(throwResult[0], 1);

// Errors while reading source elements propagate.
let throwingGetter = {
  length: 2,
  get 0() {
    return 1;
  },
  get 1() {
    throw new SyntaxError("from getter");
  },
};
assertThrowsInstanceOf(function () {
  Array.prototype.toReversed.call(throwingGetter);
}, SyntaxError);

// @@unscopables must include toReversed.
assertEq(Array.prototype[Symbol.unscopables].toReversed, true);

if (typeof reportCompare === "function") reportCompare(0, 0);
