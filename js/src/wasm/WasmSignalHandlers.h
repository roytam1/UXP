/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2014 Mozilla Foundation
 * Copyright 2021 Moonchild Productions
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_signal_handlers_h
#define wasm_signal_handlers_h

#include "mozilla/Attributes.h"
#include "threading/Thread.h"

struct JSRuntime;

namespace js {

// Force any currently-executing asm.js/ion code to call HandleExecutionInterrupt.
extern void
InterruptRunningJitCode(JSRuntime* rt);

namespace wasm {

// Ensure the given JSRuntime is set up to use signals. Failure to enable signal
// handlers indicates some catastrophic failure and creation of the runtime must
// fail.
MOZ_MUST_USE bool
EnsureSignalHandlers(JSRuntime* rt);

// Return whether signals can be used in this process for interrupts or
// asm.js/wasm out-of-bounds.
bool
HaveSignalHandlers();

// Test whether the given PC is within the innermost wasm activation. Return
// false if it is not, or it cannot be determined.
bool IsPCInWasmCode(void *pc);

} // namespace wasm
} // namespace js

#endif // wasm_signal_handlers_h
