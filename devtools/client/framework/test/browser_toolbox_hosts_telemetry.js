/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Toolbox} = require("devtools/client/framework/toolbox");
const {SIDE, BOTTOM, WINDOW} = Toolbox.HostType;

const URL = "data:text/html;charset=utf8,browser_toolbox_hosts_telemetry.js";

add_task(function* () {
  info("Create a test tab and open the toolbox");
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "webconsole");

  yield changeToolboxHost(toolbox);
  yield checkResults();
  yield toolbox.destroy();

  toolbox = target = null;
  gBrowser.removeCurrentTab();
});

function* changeToolboxHost(toolbox) {
  info("Switch toolbox host");
  yield toolbox.switchHost(SIDE);
  yield toolbox.switchHost(WINDOW);
  yield toolbox.switchHost(BOTTOM);
  yield toolbox.switchHost(SIDE);
  yield toolbox.switchHost(WINDOW);
  yield toolbox.switchHost(BOTTOM);
}

function checkResults() {
// STUB
}
