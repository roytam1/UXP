for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.toReversed.length, 0);

    var original = new constructor([1, 2, 3, 4]);
    var reversed = original.toReversed();
    assertDeepEq(reversed, new constructor([4, 3, 2, 1]));
    assertDeepEq(original, new constructor([1, 2, 3, 4]));
    assertEq(reversed === original, false);
    assertEq(reversed.constructor, constructor);

    assertDeepEq(new constructor().toReversed(), new constructor());
    assertDeepEq(new constructor([1]).toReversed(), new constructor([1]));
    assertDeepEq(new constructor([1, 2, 3]).toReversed(), new constructor([3, 2, 1]));

    var other = new constructor([7, 8, 9]);
    Object.defineProperty(other, "length", {
        get() {
            throw new Error("length accessor called");
        }
    });
    assertDeepEq(other.toReversed(), new constructor([9, 8, 7]));

    var ctorIgnored = new constructor([5, 6, 7]);
    Object.defineProperty(ctorIgnored, "constructor", {
        get() {
            throw new Error("constructor accessor called");
        }
    });
    assertDeepEq(ctorIgnored.toReversed(), new constructor([7, 6, 5]));

    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.toReversed.call(invalidReceiver);
        }, TypeError,
        "Assert that toReversed fails if this value is not a TypedArray");
    });
}

for (var constructor of typedArrayConstructors) {
    if (typeof newGlobal === "function") {
        var toReversed = newGlobal()[constructor.name].prototype.toReversed;
        var original = new constructor([3, 2, 1]);
        var reversed = toReversed.call(original);

        assertDeepEq(reversed, new constructor([1, 2, 3]));
        assertDeepEq(original, new constructor([3, 2, 1]));
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
