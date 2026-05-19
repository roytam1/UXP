var key = Symbol("weak");
var map = new WeakMap();
assertEq(map.has(key), false);
assertEq(map.get(key), undefined);
assertEq(map.set(key, 13), map);
assertEq(map.has(key), true);
assertEq(map.get(key), 13);
assertEq(map.delete(key), true);
assertEq(map.has(key), false);

var constructedKey = Symbol("constructed");
var constructed = new WeakMap([[constructedKey, 7]]);
assertEq(constructed.get(constructedKey), 7);

var registered = Symbol.for("registered");
assertEq(map.has(registered), false);
assertEq(map.get(registered), undefined);
assertEq(map.delete(registered), false);
assertThrowsInstanceOf(() => map.set(registered, 1), TypeError);
assertThrowsInstanceOf(() => new WeakMap([[registered, 1]]), TypeError);

var setKey = Symbol("set");
var set = new WeakSet([setKey]);
assertEq(set.has(setKey), true);
assertEq(set.delete(setKey), true);
assertEq(set.has(setKey), false);
assertEq(set.add(setKey), set);
assertEq(set.has(setKey), true);

assertEq(set.has(registered), false);
assertEq(set.delete(registered), false);
assertThrowsInstanceOf(() => set.add(registered), TypeError);
assertThrowsInstanceOf(() => new WeakSet([registered]), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
