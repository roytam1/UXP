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

  if (typeof BigInt64Array === "function") {
    const bigSab = new SharedArrayBuffer(16);
    const i64 = new BigInt64Array(bigSab);
    const u64 = new BigUint64Array(bigSab);

    assertEq(Atomics.store(i64, 0, -1n), -1n);
    assertEq(Atomics.load(i64, 0), -1n);
    assertEq(Atomics.load(u64, 0), 18446744073709551615n);
    assertEq(Atomics.exchange(i64, 0, 7n), -1n);
    assertEq(Atomics.compareExchange(i64, 0, 7n, 10n), 7n);
    assertEq(Atomics.add(i64, 0, 5n), 10n);
    assertEq(Atomics.load(i64, 0), 15n);
    assertEq(Atomics.sub(i64, 0, 20n), 15n);
    assertEq(Atomics.load(i64, 0), -5n);
    assertEq(Atomics.and(u64, 0, 7n), 18446744073709551611n);
    assertEq(Atomics.or(u64, 0, 8n), 3n);
    assertEq(Atomics.xor(u64, 0, 15n), 11n);

    assertThrowsInstanceOf(() => Atomics.waitAsync(u64, 0, 0n), TypeError);

    result = Atomics.waitAsync(i64, 1, 1n, 10);
    assertEq(result.async, false);
    assertEq(result.value, "not-equal");

    result = Atomics.waitAsync(i64, 1, 0n, 0);
    assertEq(result.async, false);
    assertEq(result.value, "timed-out");

    let offsetView = new BigInt64Array(bigSab, 8);
    result = Atomics.waitAsync(offsetView, 0, 0n);
    assertEq(result.async, true);
    notified = undefined;
    result.value.then(value => {
      notified = value;
    });
    assertEq(Atomics.notify(i64, 1, 1), 1);
    drainJobQueue();
    assertEq(notified, "ok");
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
