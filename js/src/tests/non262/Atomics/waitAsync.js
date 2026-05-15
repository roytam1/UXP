// |reftest| skip-if(!this.SharedArrayBuffer || !this.Atomics || !this.drainJobQueue)

if (typeof SharedArrayBuffer === "function" && typeof Atomics === "object" &&
    typeof drainJobQueue === "function") {
  const sab = new SharedArrayBuffer(4);
  const i32 = new Int32Array(sab);

  let result = Atomics.waitAsync(i32, 0, 1, 10);
  assertEq(result.async, false);
  assertEq(result.value, "not-equal");

  result = Atomics.waitAsync(i32, 0, 0, 0);
  assertEq(result.async, false);
  assertEq(result.value, "timed-out");

  result = Atomics.waitAsync(i32, 0, 0);
  assertEq(result.async, true);
  let notified;
  result.value.then(value => {
    notified = value;
  });
  assertEq(Atomics.notify(i32, 0, 1), 1);
  drainJobQueue();
  assertEq(notified, "ok");

  result = Atomics.waitAsync(i32, 0, 0, 1);
  assertEq(result.async, true);
  let timedOut;
  result.value.then(value => {
    timedOut = value;
  });
  drainJobQueue();
  assertEq(timedOut, "timed-out");
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
