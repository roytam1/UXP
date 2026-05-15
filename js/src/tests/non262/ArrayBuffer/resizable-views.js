// |reftest| skip-if(!this.SharedArrayBuffer)

var rab = new ArrayBuffer(4, { maxByteLength: 16 });
var tracking = new Uint8Array(rab);
var fixed = new Uint8Array(rab, 1, 2);
var bytes = new Uint8Array(rab);

bytes[1] = 11;
bytes[2] = 22;

assertEq(tracking.length, 4);
assertEq(tracking.byteLength, 4);
assertEq(tracking.byteOffset, 0);
assertEq(fixed.length, 2);
assertEq(fixed.byteLength, 2);
assertEq(fixed.byteOffset, 1);

rab.resize(2);
assertEq(tracking.length, 2);
assertEq(tracking.byteLength, 2);
assertEq(fixed.length, 0);
assertEq(fixed.byteLength, 0);
assertEq(fixed.byteOffset, 0);
assertEq(fixed[0], undefined);

rab.resize(8);
assertEq(tracking.length, 8);
assertEq(tracking.byteLength, 8);
assertEq(fixed.length, 2);
assertEq(fixed.byteLength, 2);
assertEq(fixed.byteOffset, 1);
assertEq(fixed[0], 11);
assertEq(fixed[1], 0);
tracking[6] = 66;
assertEq(new Uint8Array(rab)[6], 66);

var dv = new DataView(rab, 4);
assertEq(dv.byteOffset, 4);
assertEq(dv.byteLength, 4);
dv.setUint8(0, 44);
assertEq(tracking[4], 44);

rab.resize(3);
assertEq(dv.byteOffset, 0);
assertEq(dv.byteLength, 0);
assertThrowsInstanceOf(() => dv.getUint8(0), RangeError);

rab.resize(6);
assertEq(dv.byteOffset, 4);
assertEq(dv.byteLength, 2);
assertEq(dv.getUint8(0), 0);

var fixedDv = new DataView(rab, 4, 2);
rab.resize(5);
assertEq(fixedDv.byteOffset, 0);
assertEq(fixedDv.byteLength, 0);
rab.resize(6);
assertEq(fixedDv.byteOffset, 4);
assertEq(fixedDv.byteLength, 2);

var gsab = new SharedArrayBuffer(4, { maxByteLength: 16 });
var sharedTracking = new Uint8Array(gsab);
assertEq(sharedTracking.length, 4);
gsab.grow(8);
assertEq(sharedTracking.length, 8);
sharedTracking[6] = 33;
assertEq(new Uint8Array(gsab)[6], 33);

if (typeof reportCompare === "function")
  reportCompare(true, true);
