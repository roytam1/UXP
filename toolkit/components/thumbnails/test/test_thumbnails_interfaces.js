// tests to check that moz-page-thumb URLs correctly resolve as file:// URLS
"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Cr = Components.results;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");

// need profile so that PageThumbsStorage can resolve the path to the underlying file
do_get_profile();

function run_test() {
  // first check the protocol handler implements the correct interface
  let handler = Services.io.getProtocolHandler("moz-page-thumb");
  ok(handler instanceof Ci.nsISubstitutingProtocolHandler,
     "moz-page-thumb handler provides substituting interface");

  // then check that the file URL resolution works
  let uri = Services.io.newURI("moz-page-thumb://thumbnail/?url=http%3A%2F%2Fwww.mozilla.org%2F",
                               null, null);
  ok(uri instanceof Ci.nsIFileURL, "moz-page-thumb:// is a FileURL");
  ok(uri.file, "This moz-page-thumb:// object is backed by a file");
}
