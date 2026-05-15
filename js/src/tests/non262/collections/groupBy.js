// |reftest| skip-if(!Object.groupBy||!Map.groupBy)

var objectGroups = Object.groupBy(["a", "bb", "c"], value => value.length);
assertEq(Object.getPrototypeOf(objectGroups), null);
assertEqArray(objectGroups["1"], ["a", "c"]);
assertEqArray(objectGroups["2"], ["bb"]);

var symbol = Symbol();
var symbolGroups = Object.groupBy([1, 2], value => value === 1 ? symbol : "__proto__");
assertEqArray(symbolGroups[symbol], [1]);
assertEqArray(symbolGroups.__proto__, [2]);

var indexes = [];
var mapGroups = Map.groupBy(["a", "bb", "c"], (value, index) => {
  indexes.push(index);
  return value.length;
});
assertEq(mapGroups instanceof Map, true);
assertEqArray(indexes, [0, 1, 2]);
assertEqArray(mapGroups.get(1), ["a", "c"]);
assertEqArray(mapGroups.get(2), ["bb"]);

var key = {};
var objectKeyGroups = Map.groupBy([1, 2], value => value === 1 ? key : NaN);
assertEqArray(objectKeyGroups.get(key), [1]);
assertEqArray(objectKeyGroups.get(NaN), [2]);

assertThrowsInstanceOf(() => Object.groupBy(null, x => x), TypeError);
assertThrowsInstanceOf(() => Map.groupBy(undefined, x => x), TypeError);
assertThrowsInstanceOf(() => Object.groupBy([], null), TypeError);
assertThrowsInstanceOf(() => Map.groupBy([], null), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
