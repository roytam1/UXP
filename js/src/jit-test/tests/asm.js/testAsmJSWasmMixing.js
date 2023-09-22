load(libdir + "asm.js");
load(libdir + "wasm.js");

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Memory = WebAssembly.Memory;

var asmJS = asmCompile('stdlib', 'ffis', 'buf', USE_ASM + 'var i32 = new stdlib.Int32Array(buf); return {}');

var asmJSBuf = new ArrayBuffer(BUF_MIN);
asmLink(asmJS, this, null, asmJSBuf);

var wasmMem = wasmEvalText('(module (memory 1 1) (export "mem" memory))').exports.mem;
assertAsmLinkFail(asmJS, this, null, wasmMem.buffer);

