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
assertThrowsInstanceOf(() => dv.byteOffset, TypeError);
assertThrowsInstanceOf(() => dv.byteLength, TypeError);
assertThrowsInstanceOf(() => dv.getUint8(0), TypeError);

rab.resize(6);
assertEq(dv.byteOffset, 4);
assertEq(dv.byteLength, 2);
assertEq(dv.getUint8(0), 0);

var fixedDv = new DataView(rab, 4, 2);
rab.resize(5);
assertThrowsInstanceOf(() => fixedDv.byteOffset, TypeError);
assertThrowsInstanceOf(() => fixedDv.byteLength, TypeError);
assertThrowsInstanceOf(() => fixedDv.getUint8(0), TypeError);
rab.resize(6);
assertEq(fixedDv.byteOffset, 4);
assertEq(fixedDv.byteLength, 2);

var methodRab = new ArrayBuffer(4, { maxByteLength: 8 });
var methodFixed = new Uint8Array(methodRab, 2, 2);
methodRab.resize(2);
[
  () => methodFixed.at(0),
  () => methodFixed.copyWithin(0, 0),
  () => methodFixed.entries(),
  () => methodFixed.every(x => true),
  () => methodFixed.fill(1),
  () => methodFixed.filter(x => true),
  () => methodFixed.find(x => true),
  () => methodFixed.findIndex(x => true),
  () => methodFixed.findLast(x => true),
  () => methodFixed.findLastIndex(x => true),
  () => methodFixed.forEach(x => x),
  () => methodFixed.includes(0),
  () => methodFixed.indexOf(0),
  () => methodFixed.join(","),
  () => methodFixed.keys(),
  () => methodFixed.lastIndexOf(0),
  () => methodFixed.map(x => x),
  () => methodFixed.reduce((a, b) => a + b, 0),
  () => methodFixed.reduceRight((a, b) => a + b, 0),
  () => methodFixed.reverse(),
  () => methodFixed.set([1], 0),
  () => methodFixed.slice(),
  () => methodFixed.some(x => true),
  () => methodFixed.sort(),
  () => methodFixed.subarray(),
  () => methodFixed.toLocaleString(),
  () => methodFixed.toReversed(),
  () => methodFixed.toSorted(),
  () => methodFixed.toString(),
  () => methodFixed.values(),
  () => methodFixed.with(0, 1),
  () => methodFixed[Symbol.iterator](),
].forEach(fn => assertThrowsInstanceOf(fn, TypeError));

methodRab.resize(4);
assertEq(methodFixed.length, 2);

var sourceRab = new ArrayBuffer(4, { maxByteLength: 8 });
var oobSource = new Uint8Array(sourceRab, 2, 2);
sourceRab.resize(2);
assertThrowsInstanceOf(() => new Uint8Array(oobSource), TypeError);
assertThrowsInstanceOf(() => new Uint16Array(oobSource), TypeError);
assertThrowsInstanceOf(() => new Uint8Array(4).set(oobSource), TypeError);
sourceRab.resize(4);
assertEq(new Uint8Array(oobSource).length, 2);

var ctorRab = new ArrayBuffer(8, { maxByteLength: 8 });
var ShrinkingNewTarget = new Proxy(function() {}, {
  get(target, prop, receiver) {
    if (prop === "prototype") {
      ctorRab.resize(2);
      return DataView.prototype;
    }
    return Reflect.get(target, prop, receiver);
  }
});
assertThrowsInstanceOf(() => Reflect.construct(DataView, [ctorRab, 4], ShrinkingNewTarget),
                       RangeError);

var fixedCtorRab = new ArrayBuffer(8, { maxByteLength: 8 });
var FixedShrinkingNewTarget = new Proxy(function() {}, {
  get(target, prop, receiver) {
    if (prop === "prototype") {
      fixedCtorRab.resize(5);
      return DataView.prototype;
    }
    return Reflect.get(target, prop, receiver);
  }
});
assertThrowsInstanceOf(() => Reflect.construct(DataView, [fixedCtorRab, 4, 2],
                                               FixedShrinkingNewTarget),
                       RangeError);

var gsab = new SharedArrayBuffer(4, { maxByteLength: 16 });
var sharedTracking = new Uint8Array(gsab);
assertEq(sharedTracking.length, 4);
gsab.grow(8);
assertEq(sharedTracking.length, 8);
sharedTracking[6] = 33;
assertEq(new Uint8Array(gsab)[6], 33);

var sharedDv = new DataView(gsab);
assertEq(sharedDv.buffer, gsab);
assertEq(sharedDv.byteOffset, 0);
assertEq(sharedDv.byteLength, 8);
sharedDv.setUint8(7, 99);
assertEq(sharedTracking[7], 99);
gsab.grow(12);
assertEq(sharedDv.byteLength, 12);
assertEq(sharedDv.getUint8(7), 99);

var fixedSharedDv = new DataView(gsab, 4, 2);
assertEq(fixedSharedDv.byteOffset, 4);
assertEq(fixedSharedDv.byteLength, 2);
gsab.grow(16);
assertEq(fixedSharedDv.byteOffset, 4);
assertEq(fixedSharedDv.byteLength, 2);

function readLength(view) {
  return view.length;
}

function readElement(view, index) {
  return view[index];
}

function writeElement(view, index, value) {
  view[index] = value;
}

var normal = new Uint8Array(4);
normal[0] = 7;
for (var i = 0; i < 2000; i++) {
  assertEq(readLength(normal), 4);
  assertEq(readElement(normal, 0), 7);
  writeElement(normal, 1, 8);
}

var icRab = new ArrayBuffer(4, { maxByteLength: 8 });
var icTracking = new Uint8Array(icRab);
icTracking[0] = 9;
assertEq(readLength(icTracking), 4);
assertEq(readElement(icTracking, 0), 9);
icRab.resize(0);
assertEq(readLength(icTracking), 0);
assertEq(readElement(icTracking, 0), undefined);
writeElement(icTracking, 0, 1);
icRab.resize(4);
assertEq(readLength(icTracking), 4);
assertEq(readElement(icTracking, 0), 0);
writeElement(icTracking, 0, 12);
assertEq(readElement(icTracking, 0), 12);

var icFixed = new Uint8Array(icRab, 1, 2);
assertEq(readLength(icFixed), 2);
icRab.resize(2);
assertEq(readLength(icFixed), 0);
assertEq(readElement(icFixed, 0), undefined);
writeElement(icFixed, 0, 55);
icRab.resize(4);
assertEq(readLength(icFixed), 2);
assertEq(readElement(icFixed, 0), 0);

var icGsab = new SharedArrayBuffer(4, { maxByteLength: 8 });
var icSharedTracking = new Uint8Array(icGsab);
assertEq(readLength(icSharedTracking), 4);
icGsab.grow(8);
assertEq(readLength(icSharedTracking), 8);
writeElement(icSharedTracking, 5, 77);
assertEq(readElement(icSharedTracking, 5), 77);

if (typeof reportCompare === "function")
  reportCompare(true, true);
