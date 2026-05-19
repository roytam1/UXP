// |reftest| skip-if(!Promise.withResolvers)

var desc = Object.getOwnPropertyDescriptor(Promise, "withResolvers");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);
assertEq(Promise.withResolvers.length, 0);
assertEq(Promise.withResolvers.name, "withResolvers");

var capability = Promise.withResolvers();
assertEq(capability.promise instanceof Promise, true);
assertEq(typeof capability.resolve, "function");
assertEq(typeof capability.reject, "function");
assertEqArray(Object.keys(capability), ["promise", "resolve", "reject"]);

capability.resolve(42);
capability.promise.then(v => assertEq(v, 42));

class MyPromise extends Promise {}
var subCapability = Promise.withResolvers.call(MyPromise);
assertEq(subCapability.promise instanceof MyPromise, true);
subCapability.reject("rejected");
subCapability.promise.then(
  () => { throw new Error("expected rejection"); },
  reason => assertEq(reason, "rejected")
);

assertThrowsInstanceOf(() => Promise.withResolvers.call({}), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
