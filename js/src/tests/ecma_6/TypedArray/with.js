for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.with.length, 2);

    var original = new constructor([1, 2, 3, 4]);
    var updated = original.with(1, 9);
    assertDeepEq(updated, new constructor([1, 9, 3, 4]));
    assertDeepEq(original, new constructor([1, 2, 3, 4]));
    assertEq(updated === original, false);
    assertEq(updated.constructor, constructor);

    assertDeepEq(new constructor([1, 2, 3]).with(-1, 7),
                 new constructor([1, 2, 7]));
    assertDeepEq(new constructor([1, 2, 3]).with(-0, 7),
                 new constructor([7, 2, 3]));

    assertThrowsInstanceOf(() => {
        new constructor([1, 2, 3]).with(3, 9);
    }, RangeError);
    assertThrowsInstanceOf(() => {
        new constructor([1, 2, 3]).with(-4, 9);
    }, RangeError);

    var valueOrder = [];
    var value = {
        valueOf() {
            valueOrder.push("valueOf");
            return 9;
        }
    };
    assertThrowsInstanceOf(() => {
        new constructor([1, 2, 3]).with(9, value);
    }, RangeError);
    assertEq(valueOrder.join(","), "valueOf");

    var ctorIgnored = new constructor([5, 6, 7]);
    Object.defineProperty(ctorIgnored, "constructor", {
        get() {
            throw new Error("constructor accessor called");
        }
    });
    assertDeepEq(ctorIgnored.with(0, 4), new constructor([4, 6, 7]));

    if (constructor === Uint8ClampedArray ||
        (typeof isSharedConstructor === "function" && isSharedConstructor(constructor) &&
         constructor.name === Uint8ClampedArray.name))
    {
        assertDeepEq(new constructor([0, 1, 2]).with(1, 2.6),
                     new constructor([0, 3, 2]));
    }

    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.with.call(invalidReceiver, 0, 1);
        }, TypeError,
        "Assert that with fails if this value is not a TypedArray");
    });
}

for (var constructor of typedArrayConstructors) {
    if (typeof newGlobal === "function") {
        var withFn = newGlobal()[constructor.name].prototype.with;
        var original = new constructor([3, 2, 1]);
        var updated = withFn.call(original, 1, 8);

        assertDeepEq(updated, new constructor([3, 8, 1]));
        assertDeepEq(original, new constructor([3, 2, 1]));
    }
}

if (typeof BigInt64Array === "function" && typeof BigUint64Array === "function") {
    var bigIntArray = new BigInt64Array([1n, 2n, 3n]);
    assertEq(BigInt64Array.prototype.with.length, 2);
    assertDeepEq(bigIntArray.with(1, 9n), new BigInt64Array([1n, 9n, 3n]));
    assertThrowsInstanceOf(() => bigIntArray.with(1, 9), TypeError);

    var bigUintArray = new BigUint64Array([1n, 2n, 3n]);
    assertDeepEq(bigUintArray.with(2, 4n), new BigUint64Array([1n, 2n, 4n]));
    assertThrowsInstanceOf(() => bigUintArray.with(9, 1), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
