# Internal Userscripts (Polyfills Only)

This directory contains a minimal, built-in userscript loader used to ship polyfill user scripts with Basilisk. This originally started as a fork of GreaseMonkey, however at this point only a tiny XPCOM service and the bundled scripts remain.

## How it works
- The XPCOM component `@internaluserscripts.mozdev.org/service;1` observes document creation and, when `browser.internal-userscripts.enabled` is true, injects bundled polyfill scripts into each page.
- Bundled scripts live in `basilisk/internaluserscripts/bundled-scripts/` and are packaged into the app. They are loaded in the page principal and can override missing APIs.
- The only default pref is `browser.internal-userscripts.enabled` (default: true). Toggle to disable all injection.

## Bundled polyfills
- `finalizationregistry-polyfill.user.js`: best-effort FinalizationRegistry stub that immediately invokes cleanup callbacks after registration (cannot observe GC). It exposes `window.__internalUserscriptsFinalizationRegistryPolyfill = true` for verification.

## Adding new polyfills
1. Drop a `*.user.js` file into `bundled-scripts/` with the appropriate header.
2. List it in `moz.build` under `FINAL_TARGET_FILES['internal-userscripts']`.
3. The loader will inject it automatically when enabled.
