/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/" +
                 "webconsole/test/test-bug-597136-external-script-" +
                 "errors.html";

function test() {
  Task.spawn(function* () {
    const {tab} = yield loadTab(TEST_URI);
    const hud = yield openConsole(tab);

    // On e10s, the exception is triggered in child process
    // and is ignored by test harness
    if (!Services.appinfo.browserTabsRemoteAutostart) {
      expectUncaughtException();
    }
    BrowserTestUtils.synthesizeMouseAtCenter("button", {}, gBrowser.selectedBrowser);

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "bogus is not defined",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      }],
    });
  }).then(finishTest);
}
