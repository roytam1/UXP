// Manual WeakRef smoke tests for browser console use.
//
// Usage:
//   1) Paste this file into the browser console (or load via file://) and run:
//        runWeakRefManual();
//   2) Inspect the returned array of results; no throws or crashes are expected.
//
// Notes:
//   - WeakRef is always enabled. Once all strong references are gone, a full GC
//     may clear the referent and make deref() return undefined.

function runWeakRefManual() {
  const results = [];
  const log = (name, value) => results.push({ name, value });

  let target = { tag: "alpha", value: 1 };
  const ref = new WeakRef(target);

  log("initial deref tag", ref.deref()?.tag);
  log("repeat deref identity", ref.deref() === target);

  // Clear the strong reference. A future full GC may clear the WeakRef.
  target = null;

  const afterClear = ref.deref();
  log("after clearing strong ref", afterClear ? afterClear.tag : undefined);

  if (afterClear) {
    afterClear.touched = true;
  }

  log("mutation visible after deref", ref.deref()?.touched === true);

  return results;
}
