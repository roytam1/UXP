/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function assertThrowsTypeError(fn) {
  try {
    fn();
  } catch (e) {
    do_check_true(e instanceof TypeError);
    return;
  }
  do_throw("expected TypeError");
}

add_task(function* test_weakref_api_surface() {
  do_check_eq(typeof WeakRef, "function");
  do_check_eq(WeakRef.name, "WeakRef");
  do_check_eq(WeakRef.length, 1);

  assertThrowsTypeError(() => WeakRef({}));
  assertThrowsTypeError(() => new WeakRef(1));
  assertThrowsTypeError(() => new WeakRef(Symbol.for("registered")));

  let target = {};
  let ref = new WeakRef(target);
  do_check_eq(Object.prototype.toString.call(ref), "[object WeakRef]");
  do_check_eq(ref.deref(), target);
  do_check_eq(typeof WeakRef.prototype.deref, "function");
  do_check_eq(WeakRef.prototype.deref.length, 0);

  let desc = Object.getOwnPropertyDescriptor(WeakRef.prototype,
                                             Symbol.toStringTag);
  do_check_eq(desc.value, "WeakRef");
  do_check_eq(desc.writable, false);
  do_check_eq(desc.enumerable, false);
  do_check_eq(desc.configurable, true);
});

add_task(function* test_newtarget_prototype_is_not_object() {
  function newTarget() {}

  for (let proto of [undefined, null, true, "", Symbol(), 1]) {
    newTarget.prototype = proto;
    let ref = Reflect.construct(WeakRef, [{}], newTarget);
    do_check_true(Object.getPrototypeOf(ref) === WeakRef.prototype);
  }
});

add_task(function* test_proto_from_constructor_realm() {
  let other = new Components.utils.Sandbox("http://example.com", { freshZone: true });
  Components.utils.evalInSandbox("var newTarget = new Function();", other);

  for (let proto of [undefined, null, true, "", Symbol(), 1]) {
    other.newTarget.prototype = proto;
    let ref = Reflect.construct(WeakRef, [{}], other.newTarget);
    do_check_true(Object.getPrototypeOf(ref) === other.WeakRef.prototype);
  }
});
