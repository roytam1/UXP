/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test()
{
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-grips", aServer);
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    do_check_eq(args[0].class, "Object");

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getPrototypeAndProperties(function (aResponse) {
      const {a, b, c, d, e, f, g} = aResponse.ownProperties;

      testPropertyType(a, "Infinity");
      testPropertyType(b, "-Infinity");
      testPropertyType(c, "NaN");
      testPropertyType(d, "-0");
      testPropertyType(e, "BigInt");
      testPropertyType(f, "BigInt");
      testPropertyType(g, "BigInt");

      gThreadClient.resume(function () {
        gClient.close().then(gCallback);
      });
    });
  });

    gDebuggee.eval(`stopMe({
      a: Infinity,
      b: -Infinity,
      c: NaN,
      d: -0,
      e: 1n,
      f: -2n,
      g: 0n,
    })`);
}

function testPropertyType(prop, expectedType) {
  do_check_eq(prop.configurable, true);
  do_check_eq(prop.enumerable, true);
  do_check_eq(prop.writable, true);
  do_check_eq(prop.value.type, expectedType);
}
