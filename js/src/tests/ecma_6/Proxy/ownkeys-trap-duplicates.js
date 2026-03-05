/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'ownkeys-trap-duplicates.js';
var BUGNUMBER = 1293995;
var summary =
  "Scripted proxies' [[OwnPropertyKeys]] should throw if the trap " +
  "implementation returns duplicate properties";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var target = {};
var proxy = new Proxy(target, { ownKeys(t) { return ["a", "a"]; } });
assertThrowsInstanceOf(() => Object.getOwnPropertyNames(proxy), TypeError);

target = Object.preventExtensions({ a: 1 });
proxy = new Proxy(target, { ownKeys(t) { return ["a", "a"]; } });
assertThrowsInstanceOf(() => Object.getOwnPropertyNames(proxy), TypeError);

target = Object.freeze({ a: 1 });
proxy = new Proxy(target, { ownKeys(t) { return ["a", "a"]; } });
assertThrowsInstanceOf(() => Object.getOwnPropertyNames(proxy), TypeError);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
