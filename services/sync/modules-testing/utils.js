/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "btoa", // It comes from a module import.
  "encryptPayload",
  "ensureLegacyIdentityManager",
  "setBasicCredentials",
  "makeIdentityConfig",
  "configureFxAccountIdentity",
  "configureIdentity",
  "SyncTestingInfrastructure",
  "waitForZeroTimer",
  "Promise", // from a module import
  "add_identity_test",
];

var {utils: Cu} = Components;

Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://testing-common/services/common/logging.js");
Cu.import("resource://testing-common/services/sync/fakeservices.js");
Cu.import("resource://gre/modules/Promise.jsm");

/**
 * First wait >100ms (nsITimers can take up to that much time to fire, so
 * we can account for the timer in delayedAutoconnect) and then two event
 * loop ticks (to account for the Utils.nextTick() in autoConnect).
 */
this.waitForZeroTimer = function waitForZeroTimer(callback) {
  let ticks = 2;
  function wait() {
    if (ticks) {
      ticks -= 1;
      CommonUtils.nextTick(wait);
      return;
    }
    callback();
  }
  CommonUtils.namedTimer(wait, 150, {}, "timer");
}

/**
  * Ensure Sync is configured with the "legacy" identity provider.
  */
this.ensureLegacyIdentityManager = function() {
  let ns = {};
  Cu.import("resource://services-sync/service.js", ns);

  Status.__authManager = ns.Service.identity = new IdentityManager();
  ns.Service._clusterManager = ns.Service.identity.createClusterManager(ns.Service);
}

this.setBasicCredentials =
 function setBasicCredentials(username, password, syncKey) {
  let ns = {};
  Cu.import("resource://services-sync/service.js", ns);

  let auth = ns.Service.identity;
  auth.username = username;
  auth.basicPassword = password;
  auth.syncKey = syncKey;
}

// Return an identity configuration suitable for testing with our identity
// providers.  |overrides| can specify overrides for any default values.
this.makeIdentityConfig = function(overrides) {
  // first setup the defaults.
  let result = {
    // Username used in sync identity config.
    username: "foo",
    sync: {
      // username will come from the top-level username
      password: "whatever",
      syncKey: "abcdeabcdeabcdeabcdeabcdea",
    }
  };

  // Now handle any specified overrides.
  if (overrides) {
    if (overrides.username) {
      result.username = overrides.username;
    }
    if (overrides.sync) {
      // TODO: allow just some attributes to be specified
      result.sync = overrides.sync;
    }
  }
  return result;
}

this.configureIdentity = function(identityOverrides) {
  let config = makeIdentityConfig(identityOverrides);
  let ns = {};
  Cu.import("resource://services-sync/service.js", ns);

  setBasicCredentials(config.username, config.sync.password, config.sync.syncKey);
  let deferred = Promise.defer();
  deferred.resolve();
  return deferred.promise;
}

this.SyncTestingInfrastructure = function (server, username, password, syncKey) {
  let ns = {};
  Cu.import("resource://services-sync/service.js", ns);

  ensureLegacyIdentityManager();
  let config = makeIdentityConfig();
  if (username)
    config.username = username;
  if (password)
    config.sync.password = password;
  if (syncKey)
    config.sync.syncKey = syncKey;
  let cb = Async.makeSpinningCallback();
  configureIdentity(config).then(cb, cb);
  cb.wait();

  let i = server.identity;
  let uri = i.primaryScheme + "://" + i.primaryHost + ":" +
            i.primaryPort + "/";

  ns.Service.serverURL = uri;
  ns.Service.clusterURL = uri;

  this.logStats = initTestLogging();
  this.fakeFilesystem = new FakeFilesystemService({});
  this.fakeGUIDService = new FakeGUIDService();
  this.fakeCryptoService = new FakeCryptoService();
}

/**
 * Turn WBO cleartext into fake "encrypted" payload as it goes over the wire.
 */
this.encryptPayload = function encryptPayload(cleartext) {
  if (typeof cleartext == "object") {
    cleartext = JSON.stringify(cleartext);
  }

  return {
    ciphertext: cleartext, // ciphertext == cleartext with fake crypto
    IV: "irrelevant",
    hmac: fakeSHA256HMAC(cleartext, CryptoUtils.makeHMACKey("")),
  };
}

// This helper was used instead of 'add_test' or 'add_task' to run the
// specified test function twice - once with the old-style sync identity
// manager and once with the new-style BrowserID identity manager, to ensure
// it worked in both cases. Currently it's equal to just one. XXX: cleanup?
//
// * The test itself should be passed as 'test' - ie, test code will generally
//   pass |this|.
// * The test function is a regular test function - although note that it must
//   be a generator - async operations should yield them, and run_next_test
//   mustn't be called.
this.add_identity_test = function(test, testFunction) {
  function note(what) {
    let msg = "running test " + testFunction.name + " with " + what + " identity manager";
    test.do_print(msg);
  }
  let ns = {};
  Cu.import("resource://services-sync/service.js", ns);
  // one task for the "old" identity manager.
  test.add_task(function() {
    note("sync");
    let oldIdentity = Status._authManager;
    ensureLegacyIdentityManager();
    yield testFunction();
    Status.__authManager = ns.Service.identity = oldIdentity;
  });
}
