// |reftest| skip-if(!this.SharedArrayBuffer)

if (typeof SharedArrayBuffer === "function") {
  const fixed = new SharedArrayBuffer(4);
  assertEq(fixed.byteLength, 4);
  assertEq(fixed.maxByteLength, 4);
  assertEq(fixed.growable, false);
  assertThrowsInstanceOf(() => fixed.grow(4), TypeError);

  assertThrowsInstanceOf(() => new SharedArrayBuffer(-1), RangeError);
  assertThrowsInstanceOf(() => new SharedArrayBuffer(8, {maxByteLength: 4}), RangeError);

  let optionGetterCalled = false;
  const growable = new SharedArrayBuffer(4, {
    get maxByteLength() {
      optionGetterCalled = true;
      return 16;
    }
  });
  assertEq(optionGetterCalled, true);
  assertEq(growable.byteLength, 4);
  assertEq(growable.maxByteLength, 16);
  assertEq(growable.growable, true);

  const before = new Uint8Array(growable);
  before[0] = 37;
  assertEq(growable.grow(12), undefined);
  assertEq(growable.byteLength, 12);
  assertEq(growable.maxByteLength, 16);

  const after = new Uint8Array(growable);
  assertEq(after.length, 12);
  assertEq(after[0], 37);

  assertThrowsInstanceOf(() => growable.grow(11), RangeError);
  assertThrowsInstanceOf(() => growable.grow(17), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
