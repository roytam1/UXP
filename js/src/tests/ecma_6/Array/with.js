/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

assertEq(typeof Array.prototype.with, "function");
assertEq(Array.prototype.with.length, 2);

let desc = Object.getOwnPropertyDescriptor(Array.prototype, "with");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertThrowsInstanceOf(function () {
  Array.prototype.with.call(null, 0, 1);
}, TypeError);
assertThrowsInstanceOf(function () {
  Array.prototype.with.call(undefined, 0, 1);
}, TypeError);

// Non-mutating behavior.
let original = [1, 2, 3];
let updated = original.with(1, 9);
assertEq(original !== updated, true);
assertEq(original.join(","), "1,2,3");
assertEq(updated.join(","), "1,9,3");

// Negative and -0 indices.
assertEq([1, 2, 3].with(-1, 7).join(","), "1,2,7");
assertEq([1, 2, 3].with(-0, 7).join(","), "7,2,3");

// Out-of-range index throws.
assertThrowsInstanceOf(function () {
  [1, 2, 3].with(3, 9);
}, RangeError);
assertThrowsInstanceOf(function () {
  [1, 2, 3].with(-4, 9);
}, RangeError);

// Holes are materialized as undefined in the result.
let sparse = [1, , 3];
let sparseResult = sparse.with(0, 9);
assertEq(sparseResult.length, 3);
assertEq(sparseResult[0], 9);
assertEq(sparseResult[1], undefined);
assertEq(sparseResult[2], 3);
assertEq(1 in sparseResult, true);

// Generic behavior on array-like values.
let arrayLike = { 0: "a", 2: "c", length: 3 };
let arrayLikeResult = Array.prototype.with.call(arrayLike, 1, "b");
assertEq(Array.isArray(arrayLikeResult), true);
assertEq(arrayLikeResult.length, 3);
assertEq(arrayLikeResult.join(","), "a,b,c");

// Reads occur in ascending index order, except replaced index isn't read.
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
let getterResult = Array.prototype.with.call(getterArr, 1, "X");
assertEq(accessLog.join(","), "0,2");
assertEq(getterResult.join(","), "zero,X,two");

// constructor / Symbol.species is ignored and constructor is not read.
let constructorAccessed = false;
let speciesIgnored = [1, 2];
Object.defineProperty(speciesIgnored, "constructor", {
  get: function () {
    constructorAccessed = true;
    throw new Error("constructor should not be read");
  },
  configurable: true,
});
let speciesIgnoredResult = speciesIgnored.with(0, 7);
assertEq(constructorAccessed, false);
assertEq(Array.isArray(speciesIgnoredResult), true);
assertEq(speciesIgnoredResult.join(","), "7,2");

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
  Array.prototype.with.call(tooLong, 0, 1);
}, RangeError);
assertEq(indexedRead, false);

// @@unscopables must include with.
assertEq(Array.prototype[Symbol.unscopables].with, true);

if (typeof reportCompare === "function") reportCompare(0, 0);
