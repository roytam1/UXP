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

add_task(function* test_finalization_registry_api_surface() {
  do_check_eq(typeof FinalizationRegistry, "function");
  do_check_eq(FinalizationRegistry.name, "FinalizationRegistry");
  do_check_eq(FinalizationRegistry.length, 1);

  assertThrowsTypeError(() => FinalizationRegistry(function() {}));
  assertThrowsTypeError(() => new FinalizationRegistry(1));

  let registry = new FinalizationRegistry(function() {});
  do_check_eq(Object.prototype.toString.call(registry), "[object FinalizationRegistry]");
  do_check_eq(typeof FinalizationRegistry.prototype.register, "function");
  do_check_eq(FinalizationRegistry.prototype.register.length, 2);
  do_check_eq(typeof FinalizationRegistry.prototype.unregister, "function");
  do_check_eq(FinalizationRegistry.prototype.unregister.length, 1);
  do_check_eq(FinalizationRegistry.prototype.cleanupSome, undefined);

  let desc = Object.getOwnPropertyDescriptor(FinalizationRegistry.prototype,
                                             Symbol.toStringTag);
  do_check_eq(desc.value, "FinalizationRegistry");
  do_check_eq(desc.writable, false);
  do_check_eq(desc.enumerable, false);
  do_check_eq(desc.configurable, true);
});

add_task(function* test_register_validation_and_unregister() {
  let registry = new FinalizationRegistry(function() {});
  let target = {};
  let token = {};

  do_check_eq(registry.register(target, "held"), undefined);
  do_check_eq(registry.register({}, "held", token), undefined);
  do_check_eq(registry.unregister(token), true);
  do_check_eq(registry.unregister({}), false);

  assertThrowsTypeError(() => registry.register(1, "held"));
  assertThrowsTypeError(() => registry.register(target, target));
  assertThrowsTypeError(() => registry.register(target, "held", 1));
  assertThrowsTypeError(() => registry.register(Symbol.for("registered"), "held"));
  assertThrowsTypeError(() => registry.unregister(undefined));

  do_check_eq(registry.register(Symbol("target"), "held"), undefined);
  let symbolToken = Symbol("token");
  do_check_eq(registry.register({}, "held", symbolToken), undefined);
  do_check_eq(registry.unregister(symbolToken), true);
});

add_task(function* test_proto_from_constructor_realm() {
  let other = new Components.utils.Sandbox("http://example.com", { freshZone: true });
  Components.utils.evalInSandbox("var newTarget = new Function();", other);

  for (let proto of [undefined, null, true, "", Symbol(), 1]) {
    other.newTarget.prototype = proto;
    let registry = Reflect.construct(FinalizationRegistry, [function() {}],
                                     other.newTarget);
    do_check_true(Object.getPrototypeOf(registry) ===
                  other.FinalizationRegistry.prototype);
  }
});

add_task(function* test_cleanup_callback_after_gc() {
  let cleaned = [];
  let registry = new FinalizationRegistry(value => cleaned.push(value));
  let token = {};

  (function() {
    let target = {};
    registry.register(target, "held", token);
  })();

  for (let i = 0; i < 4 && cleaned.length == 0; i++) {
    Components.utils.forceGC();
    yield Promise.resolve();
  }

  do_check_eq(cleaned.length, 1);
  do_check_eq(cleaned[0], "held");
  do_check_eq(registry.unregister(token), false);
});

add_task(function* test_unregister_prevents_cleanup() {
  let cleaned = [];
  let registry = new FinalizationRegistry(value => cleaned.push(value));
  let token = {};

  (function() {
    let target = {};
    registry.register(target, "held", token);
  })();

  do_check_eq(registry.unregister(token), true);
  Components.utils.forceGC();
  yield Promise.resolve();
  do_check_eq(cleaned.length, 0);
});
