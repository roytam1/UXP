/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

assertEq(typeof Array.prototype.toSpliced, "function");
assertEq(Array.prototype.toSpliced.length, 2);

let desc = Object.getOwnPropertyDescriptor(Array.prototype, "toSpliced");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertThrowsInstanceOf(function () {
  Array.prototype.toSpliced.call(null, 0, 0);
}, TypeError);
assertThrowsInstanceOf(function () {
  Array.prototype.toSpliced.call(undefined, 0, 0);
}, TypeError);

// Non-mutating behavior.
let original = [1, 2, 3, 4];
let spliced = original.toSpliced(1, 2, "a", "b");
assertEq(original !== spliced, true);
assertEq(original.join(","), "1,2,3,4");
assertEq(spliced.join(","), "1,a,b,4");
assertEq(original.toSpliced().join(","), "1,2,3,4");

// Start index clamping and negatives.
assertEq([1, 2, 3].toSpliced(-1, 1, 9).join(","), "1,2,9");
assertEq([1, 2, 3].toSpliced(-99, 1, 9).join(","), "9,2,3");
assertEq([1, 2, 3].toSpliced(99, 1, 9).join(","), "1,2,3,9");

// deleteCount handling.
assertEq([1, 2, 3].toSpliced(1).join(","), "1");
assertEq([1, 2, 3].toSpliced(1, -3, 9).join(","), "1,9,2,3");

// Holes are materialized as undefined in copied segments.
let sparse = [1, , 3, 4];
let sparseResult = sparse.toSpliced(2, 1, "x");
assertEq(sparseResult.length, 4);
assertEq(sparseResult[0], 1);
assertEq(sparseResult[1], undefined);
assertEq(1 in sparseResult, true);
assertEq(sparseResult[2], "x");
assertEq(sparseResult[3], 4);

// Generic behavior on array-like values.
let arrayLike = { 0: "a", 1: "b", 3: "d", length: 4 };
let arrayLikeResult = Array.prototype.toSpliced.call(arrayLike, 1, 2, "X", "Y");
assertEq(Array.isArray(arrayLikeResult), true);
assertEq(arrayLikeResult.join(","), "a,X,Y,d");

// Element reads are ascending and skipped region is not read.
let accessLog = [];
let getterArr = {
  length: 5,
  get 0() {
    accessLog.push(0);
    return "z0";
  },
  get 1() {
    accessLog.push(1);
    throw new Error("must not be read");
  },
  get 2() {
    accessLog.push(2);
    throw new Error("must not be read");
  },
  get 3() {
    accessLog.push(3);
    return "z3";
  },
  get 4() {
    accessLog.push(4);
    return "z4";
  },
};
let getterResult = Array.prototype.toSpliced.call(getterArr, 1, 2, "X");
assertEq(accessLog.join(","), "0,3,4");
assertEq(getterResult.join(","), "z0,X,z3,z4");

// constructor / Symbol.species is ignored and constructor is not read.
let constructorAccessed = false;
let speciesIgnored = [1, 2, 3];
Object.defineProperty(speciesIgnored, "constructor", {
  get: function () {
    constructorAccessed = true;
    throw new Error("constructor should not be read");
  },
  configurable: true,
});
let speciesIgnoredResult = speciesIgnored.toSpliced(1, 1, 9);
assertEq(constructorAccessed, false);
assertEq(Array.isArray(speciesIgnoredResult), true);
assertEq(speciesIgnoredResult.join(","), "1,9,3");

// Length limit check happens before indexed reads.
let indexedRead = false;
let tooLong = {
  length: 4294967296,
  get 0() {
    indexedRead = true;
    throw new Error("index getter should not run");
  },
};
assertThrowsInstanceOf(function () {
  Array.prototype.toSpliced.call(tooLong, 0, 0);
}, RangeError);
assertEq(indexedRead, false);

// @@unscopables must include toSpliced.
assertEq(Array.prototype[Symbol.unscopables].toSpliced, true);

if (typeof reportCompare === "function") reportCompare(0, 0);
