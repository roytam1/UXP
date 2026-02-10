// |jit-test| module

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

// Dependency module with top-level await.
moduleRepo["dep"] = parseModule(`
  export let v = await Promise.resolve(10);
`);

// Importer module should wait for dependency evaluation.
let m = parseModule(`
  import { v } from "dep";
  export let x = v + 1;
`);

m.declarationInstantiation();

let result = m.evaluation();
assertEq(result instanceof Promise, true);

// Resolve the module evaluation promise and verify bindings are initialized.
drainJobQueue();
assertEq(getModuleEnvironmentValue(moduleRepo["dep"], "v"), 10);
assertEq(getModuleEnvironmentValue(m, "x"), 11);

// Re-evaluation after completion should return undefined.
assertEq(typeof m.evaluation(), "undefined");
