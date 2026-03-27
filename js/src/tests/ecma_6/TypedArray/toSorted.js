for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.toSorted.length, 1);

    var original = new constructor([3, 1, 2]);
    var sorted = original.toSorted();
    assertDeepEq(sorted, new constructor([1, 2, 3]));
    assertDeepEq(original, new constructor([3, 1, 2]));
    assertEq(sorted === original, false);
    assertEq(sorted.constructor, constructor);

    var descending = original.toSorted((a, b) => b - a);
    assertDeepEq(descending, new constructor([3, 2, 1]));
    assertDeepEq(original, new constructor([3, 1, 2]));

    var stableInput = new constructor([2, 1, 4, 3]);
    var stableSorted = stableInput.toSorted((a, b) => (a % 2) - (b % 2));
    assertDeepEq(stableSorted, new constructor([2, 4, 1, 3]));

    var nanComparatorInput = new constructor([4, 1, 3, 2]);
    var nanComparatorSorted = nanComparatorInput.toSorted(() => NaN);
    assertDeepEq(nanComparatorSorted, nanComparatorInput);

    var ctorIgnored = new constructor([5, 1, 4]);
    Object.defineProperty(ctorIgnored, "constructor", {
        get() {
            throw new Error("constructor accessor called");
        }
    });
    assertDeepEq(ctorIgnored.toSorted(), new constructor([1, 4, 5]));

    if (isFloatConstructor(constructor)) {
        var floatInput = new constructor([0, -0, 1, -1, NaN]);
        var floatSorted = floatInput.toSorted();

        assertEq(floatSorted[0], -1);
        assertEq(1 / floatSorted[1], -Infinity);
        assertEq(1 / floatSorted[2], Infinity);
        assertEq(floatSorted[3], 1);
        assertEq(Number.isNaN(floatSorted[4]), true);
    }

    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.toSorted.call(invalidReceiver);
        }, TypeError,
        "Assert that toSorted fails if this value is not a TypedArray");
    });
}

for (var constructor of typedArrayConstructors) {
    if (typeof newGlobal === "function") {
        var toSorted = newGlobal()[constructor.name].prototype.toSorted;
        var original = new constructor([3, 2, 1]);
        var sorted = toSorted.call(original);

        assertDeepEq(sorted, new constructor([1, 2, 3]));
        assertDeepEq(original, new constructor([3, 2, 1]));
    }
}

assertThrowsInstanceOf(() => {
    Int32Array.prototype.toSorted.call(new Int32Array([1, 2, 3]), 0);
}, TypeError);

if (typeof detachArrayBuffer === "function") {
    assertThrowsInstanceOf(() => {
        let buffer = new ArrayBuffer(16);
        let array = new Int32Array(buffer);
        detachArrayBuffer(buffer);
        array.toSorted();
    }, TypeError);
}

if (typeof detachArrayBuffer === "function") {
    let ta = new Int32Array([3, 1, 2]);
    let detached = false;
    let sorted = ta.toSorted(function(a, b) {
        if (!detached) {
            detached = true;
            detachArrayBuffer(ta.buffer);
        }
        return a - b;
    });
    assertDeepEq(sorted, new Int32Array([1, 2, 3]));
}

if (typeof newGlobal === "function" && typeof detachArrayBuffer === "function") {
    let ta = new Int32Array([3, 1, 2]);
    let otherGlobal = newGlobal();
    let detached = false;
    let sorted = otherGlobal.Int32Array.prototype.toSorted.call(ta, function(a, b) {
        if (!detached) {
            detached = true;
            detachArrayBuffer(ta.buffer);
        }
        return a - b;
    });
    assertDeepEq(sorted, new Int32Array([1, 2, 3]));
}

if (typeof BigInt64Array === "function" && typeof BigUint64Array === "function") {
    let bigIntArray = new BigInt64Array([3n, 1n, 2n]);
    let bigIntSorted = bigIntArray.toSorted();
    assertEq(BigInt64Array.prototype.toSorted.length, 1);
    assertDeepEq(bigIntSorted, new BigInt64Array([1n, 2n, 3n]));
    assertDeepEq(bigIntArray, new BigInt64Array([3n, 1n, 2n]));

    assertThrowsInstanceOf(() => {
        bigIntArray.toSorted((a, b) => a - b);
    }, TypeError);

    let bigUintArray = new BigUint64Array([3n, 1n, 2n]);
    assertDeepEq(bigUintArray.toSorted(), new BigUint64Array([1n, 2n, 3n]));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
