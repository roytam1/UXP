// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
// ==UserScript==
// @name         WebAuthn Shim for Microsoft
// @namespace    internal-userscripts
// @description  Forces WebAuthn capability checks to report unsupported on Microsoft auth domains.
// @match        https://*.microsoft.com/*
// @match        https://*.live.com/*
// @match        https://*.msauth.net/*
// @match        https://*.visualstudio.com/*
// @grant        none
// @run-at       document-start
// ==/UserScript==
(function () {
  "use strict";

  var global = typeof window !== "undefined" ? window : null;
  if (!global || !global.location) {
    return;
  }

  if (global.location.protocol !== "https:") {
    return;
  }

  var host = String(global.location.hostname || "").toLowerCase();
  function matchesDomain(domain) {
    return host === domain || host.slice(-(domain.length + 1)) === "." + domain;
  }

  if (
    !matchesDomain("microsoft.com") &&
    !matchesDomain("live.com") &&
    !matchesDomain("msauth.net") &&
    !matchesDomain("visualstudio.com")
  ) {
    return;
  }

  function installShim() {
    if (typeof navigator.credentials === "undefined") {
      var fake = {
        create: function () {
          return Promise.reject(
            new DOMException("WebAuthn not supported", "NotSupportedError"),
          );
        },
        get: function () {
          return Promise.reject(
            new DOMException("WebAuthn not supported", "NotSupportedError"),
          );
        },
      };

      try {
        navigator.credentials = fake;
      } catch (e) {
        try {
          Object.defineProperty(navigator, "credentials", {
            value: fake,
          });
        } catch (e2) {}
      }
    }

    if (typeof global.PublicKeyCredential === "undefined") {
      global.PublicKeyCredential = function () {};
      global.PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable =
        function () {
          return Promise.resolve(false);
        };
    }
  }

  installShim();
  try {
    global.__internalUserscriptsWebAuthnMicrosoftShim = true;
  } catch (e) {}
})();
