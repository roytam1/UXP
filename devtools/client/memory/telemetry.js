/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module exports methods to record telemetry data for memory tool usage.
//
// NB: Ensure that *every* exported function is wrapped in `makeInfallible` so
// that our probes don't accidentally break code that actually does productive
// work for the user!

const { telemetry } = require("Services");
const { makeInfallible, immutableUpdate } = require("devtools/shared/DevToolsUtils");
const { labelDisplays, treeMapDisplays, censusDisplays } = require("./constants");

exports.countTakeSnapshot = makeInfallible(function () {
}, "devtools/client/memory/telemetry#countTakeSnapshot");

exports.countImportSnapshot = makeInfallible(function () {
}, "devtools/client/memory/telemetry#countImportSnapshot");

exports.countExportSnapshot = makeInfallible(function () {
}, "devtools/client/memory/telemetry#countExportSnapshot");

const COARSE_TYPE = "Coarse Type";
const ALLOCATION_STACK = "Allocation Stack";
const INVERTED_ALLOCATION_STACK = "Inverted Allocation Stack";
const CUSTOM = "Custom";

/**
 * @param {String|null} filter
 *        The filter string used, if any.
 *
 * @param {Boolean} diffing
 *        True if the census was a diffing census, false otherwise.
 *
 * @param {censusDisplayModel} display
 *        The display used with the census.
 */
exports.countCensus = makeInfallible(function ({ filter, diffing, display }) {
}, "devtools/client/memory/telemetry#countCensus");

/**
 * @param {Object} opts
 *        The same parameters specified for countCensus.
 */
exports.countDiff = makeInfallible(function (opts) {
  exports.countCensus(immutableUpdate(opts, { diffing: true }));
}, "devtools/client/memory/telemetry#countDiff");

/**
 * @param {Object} display
 *        The display used to label nodes in the dominator tree.
 */
exports.countDominatorTree = makeInfallible(function ({ display }) {
}, "devtools/client/memory/telemetry#countDominatorTree");
