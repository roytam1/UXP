/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(typeof Object.groupBy, "function");
assertEq(Object.groupBy.length, 2);

let desc = Object.getOwnPropertyDescriptor(Object, "groupBy");
assertEq(desc.enumerable, false);
assertEq(desc.writable, true);
assertEq(desc.configurable, true);

{
    let values = [1, 2, 3, 4, 5];
    let calls = [];

    let grouped = Object.groupBy(values, function (value, index) {
        "use strict";
        assertEq(this, undefined);
        calls.push([value, index]);
        return value % 2 ? "odd" : "even";
    });

    assertEq(Object.getPrototypeOf(grouped), null);
    assertDeepEq(Object.keys(grouped), ["odd", "even"]);
    assertDeepEq(grouped.odd, [1, 3, 5]);
    assertDeepEq(grouped.even, [2, 4]);
    assertDeepEq(calls, [[1, 0], [2, 1], [3, 2], [4, 3], [5, 4]]);
}

{
    let sym = Symbol("key");
    let grouped = Object.groupBy([10, 11], value => value === 10 ? sym : 0);

    assertDeepEq(Object.keys(grouped), ["0"]);
    assertDeepEq(grouped[0], [11]);

    let symbols = Object.getOwnPropertySymbols(grouped);
    assertEq(symbols.length, 1);
    assertEq(symbols[0], sym);
    assertDeepEq(grouped[sym], [10]);
}

{
    let grouped = Object.groupBy([1, 2], () => "__proto__");
    assertEq(Object.getPrototypeOf(grouped), null);
    assertDeepEq(grouped["__proto__"], [1, 2]);

    let protoKeyDesc = Object.getOwnPropertyDescriptor(grouped, "__proto__");
    assertEq(protoKeyDesc.enumerable, true);
    assertEq(protoKeyDesc.writable, true);
    assertEq(protoKeyDesc.configurable, true);
}

{
    let closed = false;
    let iterable = {
        [Symbol.iterator]() {
            let i = 0;
            return {
                next() {
                    i++;
                    if (i <= 3)
                        return { value: i, done: false };
                    return { done: true };
                },
                return() {
                    closed = true;
                    return { done: true };
                }
            };
        }
    };

    assertThrowsValue(() => Object.groupBy(iterable, value => {
        if (value === 2)
            throw 42;
        return "x";
    }), 42);
    assertEq(closed, true);
}

assertThrowsInstanceOf(() => Object.groupBy([], null), TypeError);
assertThrowsInstanceOf(() => Object.groupBy(null, null), TypeError);
assertThrowsInstanceOf(() => Object.groupBy({}, x => x), TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
