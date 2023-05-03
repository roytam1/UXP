/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FunctionEmitter_h
#define frontend_FunctionEmitter_h

#include "mozilla/Attributes.h"  // MOZ_STACK_CLASS, MOZ_MUST_USE

#include <stdint.h>  // uint16_t, uint32_t

#include "jsopcode.h"
#include "jsfun.h"                         // JSFunction

#include "frontend/DefaultEmitter.h"       // DefaultEmitter
#include "frontend/DestructuringFlavor.h"  // DestructuringFlavor
#include "frontend/EmitterScope.h"         // EmitterScope
#include "frontend/SharedContext.h"        // FunctionBox
#include "frontend/TDZCheckCache.h"        // TDZCheckCache
#include "gc/Rooting.h"                    // JS::Rooted, JS::Handle
#include "vm/String.h"                     // JSAtom

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting function declaration, expression, or method etc.
//
// This class handles the enclosing script's part (function object creation,
// declaration, etc). The content of the function script is handled by
// FunctionScriptEmitter and FunctionParamsEmitter.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `function f() {}`, non lazy script
//     FunctionEmitter fe(this, funbox_for_f, FunctionSyntaxKind::Statement,
//                        false);
//     fe.prepareForNonLazy();
//
//     // Emit script with FunctionScriptEmitter here.
//     ...
//
//     fe.emitNonLazyEnd();
//
//   `function f() {}`, lazy script
//     FunctionEmitter fe(this, funbox_for_f, FunctionSyntaxKind::Statement,
//                        false);
//     fe.emitLazy();
//
//   `function f() {}`, emitting hoisted function again
//     // See emitAgain comment for more details
//     FunctionEmitter fe(this, funbox_for_f, FunctionSyntaxKind::Statement,
//                        true);
//     fe.emitAgain();
//
//   `function f() { "use asm"; }`
//     FunctionEmitter fe(this, funbox_for_f, FunctionSyntaxKind::Statement,
//                        false);
//     fe.emitAsmJSModule();
//
class MOZ_STACK_CLASS FunctionEmitter {
  private:
    BytecodeEmitter* bce_;

    FunctionBox* funbox_;

    // Function linked from funbox_.
    JS::Rooted<JSFunction*> fun_;

    // Function's explicit name.
    JS::Rooted<JSAtom*> name_;

    FunctionSyntaxKind syntaxKind_;
    bool isHoisted_;

#ifdef DEBUG
    // The state of this emitter.
    //
    // +-------+
    // | Start |-+
    // +-------+ |
    //           |
    //   +-------+
    //   |
    //   | [non-lazy function]
    //   |   prepareForNonLazy  +---------+ emitNonLazyEnd     +-----+
    //   +--------------------->| NonLazy |---------------->+->| End |
    //   |                      +---------+                 ^  +-----+
    //   |                                                  |
    //   | [lazy function]                                  |
    //   |   emitLazy                                       |
    //   +------------------------------------------------->+
    //   |                                                  ^
    //   | [emitting hoisted function again]                |
    //   |   emitAgain                                      |
    //   +------------------------------------------------->+
    //   |                                                  ^
    //   | [asm.js module]                                  |
    //   |   emitAsmJSModule                                |
    //   +--------------------------------------------------+
    //
    enum class State {
        // The initial state.
        Start,

        // After calling prepareForNonLazy.
        NonLazy,

        // After calling emitNonLazyEnd, emitLazy, emitAgain, or emitAsmJSModule.
        End
    };
    State state_ = State::Start;
#endif

  public:
    FunctionEmitter(BytecodeEmitter* bce, FunctionBox* funbox,
                    FunctionSyntaxKind syntaxKind, bool isHoisted);

    MOZ_MUST_USE bool prepareForNonLazy();
    MOZ_MUST_USE bool emitNonLazyEnd();

    MOZ_MUST_USE bool emitLazy();

    MOZ_MUST_USE bool emitAgain();

    MOZ_MUST_USE bool emitAsmJSModule();

  private:
    // Common code for non-lazy and lazy functions.
    MOZ_MUST_USE bool interpretedCommon();

    // Emit the function declaration, expression, method etc.
    // This leaves function object on the stack for expression etc,
    // and doesn't for declaration.
    MOZ_MUST_USE bool emitFunction();

    // Helper methods used by emitFunction for each case.
    // `index` is the object index of the function.
    MOZ_MUST_USE bool emitNonHoisted(unsigned index);
    MOZ_MUST_USE bool emitHoisted(unsigned index);
    MOZ_MUST_USE bool emitTopLevelFunction(unsigned index);
    MOZ_MUST_USE bool emitNewTargetForArrow();
};

// Class for emitting function script.
// Parameters are handled by FunctionParamsEmitter.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `function f(a) { expr }`
//     FunctionScriptEmitter fse(this, funbox_for_f,
//                               Some(offset_of_opening_paren),
//                               Some(offset_of_closing_brace));
//     fse.prepareForParameters();
//
//     // Emit parameters with FunctionParamsEmitter here.
//     ...
//
//     fse.prepareForBody();
//     emit(expr);
//     fse.emitEnd();
//
//     // Do NameFunctions operation here if needed.
//
//     fse.initScript();
//
class MOZ_STACK_CLASS FunctionScriptEmitter {
  private:
    BytecodeEmitter* bce_;

    FunctionBox* funbox_;

    // Scope for the function name for a named lambda.
    // None for anonymous function.
    mozilla::Maybe<EmitterScope> namedLambdaEmitterScope_;

    // Scope for function body.
    mozilla::Maybe<EmitterScope> functionEmitterScope_;

    // Scope for the extra body var.
    // None if `funbox_->hasExtraBodyVarScope() == false`.
    mozilla::Maybe<EmitterScope> extraBodyVarEmitterScope_;

    mozilla::Maybe<TDZCheckCache> tdzCache_;

    // See the comment for constructor.
    mozilla::Maybe<uint32_t> paramStart_;
    mozilla::Maybe<uint32_t> bodyEnd_;

#ifdef DEBUG
    // The state of this emitter.
    //
    // +-------+ prepareForParameters  +------------+
    // | Start |---------------------->| Parameters |-+
    // +-------+                       +------------+ |
    //                                                |
    //   +--------------------------------------------+
    //   |
    //   | prepareForBody  +------+ emitEndBody  +---------+
    //   +---------------->| Body |------------->| EndBody |-+
    //                     +------+              +---------+ |
    //                                                       |
    //     +-------------------------------------------------+
    //     |
    //     | initScript  +-----+
    //     +------------>| End |
    //                   +-----+
    enum class State {
        // The initial state.
        Start,

        // After calling prepareForParameters.
        Parameters,

        // After calling prepareForBody.
        Body,

        // After calling emitEndBody.
        EndBody,

        // After calling initScript.
        End
    };
    State state_ = State::Start;
#endif

  public:
    // Parameters are the offset in the source code for each character below:
    //
    //   function f(a, b, ...c) { ... }
    //             ^                  ^
    //             |                  |
    //             paramStart         bodyEnd
    //
    // Can be Nothing() if not available.
    FunctionScriptEmitter(BytecodeEmitter* bce, FunctionBox* funbox,
                          const mozilla::Maybe<uint32_t>& paramStart,
                          const mozilla::Maybe<uint32_t>& bodyEnd)
        : bce_(bce),
          funbox_(funbox),
          paramStart_(paramStart),
          bodyEnd_(bodyEnd) {}

    MOZ_MUST_USE bool prepareForParameters();
    MOZ_MUST_USE bool prepareForBody();
    MOZ_MUST_USE bool emitEndBody();

    // Initialize JSScript for this function.
    // WARNING: There shouldn't be any fallible operation for the function
    //          compilation after `initScript` call.
    //          See the comment inside JSScript::fullyInitFromEmitter for
    //          more details.
    MOZ_MUST_USE bool initScript();

 private:
    MOZ_MUST_USE bool emitExtraBodyVarScope();
};

// Class for emitting function parameters.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `function f(a, b=10, ...c) {}`
//     FunctionParamsEmitter fpe(this, funbox_for_f);
//
//     fpe.emitSimple(atom_of_a);
//
//     fpe.prepareForDefault();
//     emit(10);
//     fpe.emitDefaultEnd(atom_of_b);
//
//     fpe.emitRest(atom_of_c);
//
//   `function f([a], [b]=[1], ...[c]) {}`
//     FunctionParamsEmitter fpe(this, funbox_for_f);
//
//     fpe.prepareForDestructuring();
//     emit(destructuring_for_[a]);
//     fpe.emitDestructuringEnd();
//
//     fpe.prepareForDestructuringDefaultInitializer();
//     emit([1]);
//     fpe.prepareForDestructuringDefault();
//     emit(destructuring_for_[b]);
//     fpe.emitDestructuringDefaultEnd();
//
//     fpe.prepareForDestructuringRest();
//     emit(destructuring_for_[c]);
//     fpe.emitDestructuringRestEnd();
//
class MOZ_STACK_CLASS FunctionParamsEmitter {
  private:
    BytecodeEmitter* bce_;

    FunctionBox* funbox_;

    // The pointer to `FunctionScriptEmitter::functionEmitterScope_`,
    // passed via `BytecodeEmitter::innermostEmitterScope()`.
    EmitterScope* functionEmitterScope_;

    // The slot for the current parameter.
    // NOTE: after emitting rest parameter, this isn't incremented.
    uint16_t argSlot_ = 0;

    // DefaultEmitter for default parameter.
    mozilla::Maybe<DefaultEmitter> default_;

    // Scope for each parameter expression.
    // Populated only when there's `eval` in parameters.
    mozilla::Maybe<EmitterScope> paramExprVarEmitterScope_;

#ifdef DEBUG
    // The state of this emitter.
    //
    // +----------------------------------------------------------+
    // |                                                          |
    // |  +-------+                                               |
    // +->| Start |-+                                             |
    //    +-------+ |                                             |
    //              |                                             |
    // +------------+                                             |
    // |                                                          |
    // | [single binding, wihtout default]                        |
    // |   emitSimple                                             |
    // +--------------------------------------------------------->+
    // |                                                          ^
    // | [single binding, with default]                           |
    // |   prepareForDefault  +---------+ emitDefaultEnd          |
    // +--------------------->| Default |------------------------>+
    // |                      +---------+                         ^
    // |                                                          |
    // | [destructuring, without default]                         |
    // |   prepareForDestructuring  +---------------+             |
    // +--------------------------->| Destructuring |-+           |
    // |                            +---------------+ |           |
    // |                                              |           |
    // |    +-----------------------------------------+           |
    // |    |                                                     |
    // |    | emitDestructuringEnd                                |
    // |    +---------------------------------------------------->+
    // |                                                          ^
    // | [destructuring, with default]                            |
    // |   prepareForDestructuringDefaultInitializer              |
    // +---------------------------------------------+            |
    // |                                             |            |
    // |    +----------------------------------------+            |
    // |    |                                                     |
    // |    |  +---------------------------------+                |
    // |    +->| DestructuringDefaultInitializer |-+              |
    // |       +---------------------------------+ |              |
    // |                                           |              |
    // |      +------------------------------------+              |
    // |      |                                                   |
    // |      | prepareForDestructuringDefault                    |
    // |      +-------------------------------+                   |
    // |                                      |                   |
    // |        +-----------------------------+                   |
    // |        |                                                 |
    // |        |  +----------------------+                       |
    // |        +->| DestructuringDefault |-+                     |
    // |           +----------------------+ |                     |
    // |                                    |                     |
    // |          +-------------------------+                     |
    // |          |                                               |
    // |          | emitDestructuringDefaultEnd                   |
    // |          +---------------------------------------------->+
    // |
    // | [single binding rest]
    // |   emitRest                                                  +-----+
    // +--------------------------------------------------------->+->| End |
    // |                                                          ^  +-----+
    // | [destructuring rest]                                     |
    // |   prepareForDestructuringRest   +-------------------+    |
    // +-------------------------------->| DestructuringRest |-+  |
    //                                   +-------------------+ |  |
    //                                                         |  |
    //    +----------------------------------------------------+  |
    //    |                                                       |
    //    | emitDestructuringRestEnd                              |
    //    +-------------------------------------------------------+
    //
    enum class State {
        // The initial state, or after emitting non-rest parameter.
        Start,

        // After calling prepareForDefault.
        Default,

        // After calling prepareForDestructuring.
        Destructuring,

        // After calling prepareForDestructuringDefaultInitializer.
        DestructuringDefaultInitializer,

        // After calling prepareForDestructuringDefault.
        DestructuringDefault,

        // After calling prepareForDestructuringRest.
        DestructuringRest,

        // After calling emitRest or emitDestructuringRestEnd.
        End,
    };
    State state_ = State::Start;
#endif

  public:
    FunctionParamsEmitter(BytecodeEmitter* bce, FunctionBox* funbox);

    // paramName is used only when there's at least one expression in the
    // paramerters (funbox_->hasParameterExprs == true).
    MOZ_MUST_USE bool emitSimple(JS::Handle<JSAtom*> paramName);

    MOZ_MUST_USE bool prepareForDefault();
    MOZ_MUST_USE bool emitDefaultEnd(JS::Handle<JSAtom*> paramName);

    MOZ_MUST_USE bool prepareForDestructuring();
    MOZ_MUST_USE bool emitDestructuringEnd();

    MOZ_MUST_USE bool prepareForDestructuringDefaultInitializer();
    MOZ_MUST_USE bool prepareForDestructuringDefault();
    MOZ_MUST_USE bool emitDestructuringDefaultEnd();

    MOZ_MUST_USE bool emitRest(JS::Handle<JSAtom*> paramName);

    MOZ_MUST_USE bool prepareForDestructuringRest();
    MOZ_MUST_USE bool emitDestructuringRestEnd();

    MOZ_MUST_USE DestructuringFlavor getDestructuringFlavor();

  private:
    // Enter/leave var scope for `eval` if necessary.
    MOZ_MUST_USE bool enterParameterExpressionVarScope();
    MOZ_MUST_USE bool leaveParameterExpressionVarScope();

    MOZ_MUST_USE bool prepareForInitializer();
    MOZ_MUST_USE bool emitInitializerEnd();

    MOZ_MUST_USE bool emitRestArray();

    MOZ_MUST_USE bool emitAssignment(JS::Handle<JSAtom*> paramName);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_FunctionEmitter_h */
