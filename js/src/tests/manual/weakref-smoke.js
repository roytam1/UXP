// Manual WeakRef smoke tests for browser console use.
//
// Usage:
//   1) Optionally flip `javascript.options.weakrefs` in about:config and reload.
//   2) Paste this file into the browser console (or load via file://) and run:
//        runWeakRefManual();
//   3) Inspect the returned array of results; no throws or crashes are expected.
//
// Notes:
//   - When the pref is ON, deref() should return the target.
//   - When the pref is OFF, the stub traces strongly so deref() should also
//     return the target.

function runWeakRefManual() {
  const results = [];
  const log = (name, value) => results.push({ name, value });

  let target = { tag: "alpha", value: 1 };
  const ref = new WeakRef(target);

  log("initial deref tag", ref.deref()?.tag);
  log("repeat deref identity", ref.deref() === target);

  // Clear the strong reference; the WeakRef should still be able to return it.
  target = null;

  const afterClear = ref.deref();
  log("after clearing strong ref", afterClear ? afterClear.tag : undefined);

  if (afterClear) {
    afterClear.touched = true;
  }

  log("mutation visible after deref", ref.deref()?.touched === true);

  return results;
}
