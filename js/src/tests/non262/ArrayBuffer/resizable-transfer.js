// |reftest| skip-if(!ArrayBuffer.prototype.transfer)

var fixed = new ArrayBuffer(4);
assertEq(fixed.byteLength, 4);
assertEq(fixed.maxByteLength, 4);
assertEq(fixed.resizable, false);
assertEq(fixed.detached, false);
assertThrowsInstanceOf(() => fixed.resize(2), TypeError);
assertEq(ArrayBuffer.prototype.transfer.length, 0);
assertEq(ArrayBuffer.prototype.transferToFixedLength.length, 0);
var resizeArgumentConverted = false;
assertThrowsInstanceOf(() => fixed.resize({ valueOf() { resizeArgumentConverted = true; return 1; } }),
                       TypeError);
assertEq(resizeArgumentConverted, false);

var resizable = new ArrayBuffer(4, { maxByteLength: 8 });
assertEq(resizable.byteLength, 4);
assertEq(resizable.maxByteLength, 8);
assertEq(resizable.resizable, true);

var bytes = new Uint8Array(resizable);
bytes[0] = 11;
bytes[3] = 44;
resizable.resize(6);
assertEq(resizable.byteLength, 6);
assertEq(new Uint8Array(resizable)[0], 11);
assertEq(new Uint8Array(resizable)[3], 44);
assertEq(new Uint8Array(resizable)[4], 0);
assertThrowsInstanceOf(() => resizable.resize(9), RangeError);

var source = new ArrayBuffer(4);
var sourceBytes = new Uint8Array(source);
sourceBytes[0] = 1;
sourceBytes[1] = 2;
var sourceView = new Uint8Array(source);
var moved = source.transfer(6);
assertEq(source.detached, true);
assertEq(source.byteLength, 0);
assertEq(source.maxByteLength, 0);
assertEq(sourceView.length, 0);
assertEq(moved.byteLength, 6);
assertEq(moved.resizable, false);
assertEq(moved.maxByteLength, 6);
assertEq(new Uint8Array(moved)[0], 1);
assertEq(new Uint8Array(moved)[1], 2);
assertEq(new Uint8Array(moved)[4], 0);

var resizableSource = new ArrayBuffer(4, { maxByteLength: 8 });
new Uint8Array(resizableSource)[0] = 7;
var resizableMoved = resizableSource.transfer();
assertEq(resizableSource.detached, true);
assertEq(resizableSource.resizable, true);
assertEq(resizableMoved.byteLength, 4);
assertEq(resizableMoved.maxByteLength, 8);
assertEq(resizableMoved.resizable, true);
assertEq(new Uint8Array(resizableMoved)[0], 7);

var fixedMoved = resizableMoved.transferToFixedLength(10);
assertEq(resizableMoved.detached, true);
assertEq(fixedMoved.byteLength, 10);
assertEq(fixedMoved.maxByteLength, 10);
assertEq(fixedMoved.resizable, false);
assertEq(new Uint8Array(fixedMoved)[0], 7);

var sliceSource = new ArrayBuffer(8, { maxByteLength: 8 });
var sliceSourceBytes = new Uint8Array(sliceSource);
for (var i = 0; i < sliceSourceBytes.length; i++)
  sliceSourceBytes[i] = i + 1;

sliceSource.constructor = {
  [Symbol.species]: function(byteLength) {
    sliceSource.resize(4);
    return new ArrayBuffer(byteLength);
  }
};

var sliced = sliceSource.slice(2, 8);
var slicedBytes = new Uint8Array(sliced);
assertEq(sliced.byteLength, 6);
assertEq(slicedBytes[0], 3);
assertEq(slicedBytes[1], 4);
assertEq(slicedBytes[2], 0);
assertEq(slicedBytes[5], 0);

sliceSource.resize(8);
for (var i = 0; i < sliceSourceBytes.length; i++)
  sliceSourceBytes[i] = i + 1;
var zeroCopied = sliceSource.slice(6, 8);
assertEq(zeroCopied.byteLength, 2);
assertEq(new Uint8Array(zeroCopied)[0], 0);

assertThrowsInstanceOf(() => new ArrayBuffer(4, { maxByteLength: 3 }), RangeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
