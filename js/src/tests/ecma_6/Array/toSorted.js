/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

assertEq(typeof Array.prototype.toSorted, "function");

// Non-mutating behavior.
let original = [3, 1, 2];
let sorted = original.toSorted();
assertEq(original !== sorted, true);
assertEq(original.join(","), "3,1,2");
assertEq(sorted.join(","), "1,2,3");

// Compare function.
let nums = [10, 1, 5];
let desc = nums.toSorted((a, b) => b - a);
assertEq(desc.join(","), "10,5,1");

// Stable sort.
let stableInput = [
    {v: 1, id: "a"},
    {v: 1, id: "b"},
    {v: 1, id: "c"}
];
let stableSorted = stableInput.toSorted((x, y) => x.v - y.v);
assertEq(stableSorted.map(o => o.id).join(""), "abc");

// Holes are treated as undefined (properties are created).
let sparse = [3, , 1];
let sparseSorted = sparse.toSorted();
assertEq(sparseSorted.length, 3);
assertEq(sparseSorted[0], 1);
assertEq(sparseSorted[1], 3);
assertEq(2 in sparseSorted, true);
assertEq(sparseSorted[2], undefined);

// Array-like input.
let arrayLike = {0: 2, 1: 1, length: 2};
let arrayLikeSorted = Array.prototype.toSorted.call(arrayLike);
assertEq(Array.isArray(arrayLikeSorted), true);
assertEq(arrayLikeSorted.join(","), "1,2");

// Getter access order (ascending indices).
let accessLog = [];
let getterArr = {
    length: 3,
    get 0() { accessLog.push(0); return 3; },
    get 1() { accessLog.push(1); return 1; },
    get 2() { accessLog.push(2); return 2; }
};
Array.prototype.toSorted.call(getterArr);
assertEq(accessLog.join(","), "0,1,2");

// Comparator errors propagate.
assertThrowsInstanceOf(() => [1, 2].toSorted(1), TypeError);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
