/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_TryEmitter_h
#define frontend_TryEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stddef.h>
#include <stdint.h>

#include "frontend/BytecodeControlStructures.h"
#include "frontend/JumpList.h"
#include "frontend/TDZCheckCache.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

class MOZ_STACK_CLASS TryEmitter
{
  public:
    enum Kind {
        TryCatch,
        TryCatchFinally,
        TryFinally
    };
    enum ShouldUseRetVal {
        UseRetVal,
        DontUseRetVal
    };
    enum ShouldUseControl {
        UseControl,
        DontUseControl,
    };

  private:
    BytecodeEmitter* bce_;
    Kind kind_;
    ShouldUseRetVal retValKind_;

    // Track jumps-over-catches and gosubs-to-finally for later fixup.
    //
    // When a finally block is active, non-local jumps (including
    // jumps-over-catches) result in a GOSUB being written into the bytecode
    // stream and fixed-up later.
    //
    // If ShouldUseControl is DontUseControl, all that handling is skipped.
    // DontUseControl is used by yield* and the internal try-catch around
    // IteratorClose. These internal uses must:
    //   * have only one catch block
    //   * have no catch guard
    //   * have JSOP_GOTO at the end of catch-block
    //   * have no non-local-jump
    //   * don't use finally block for normal completion of try-block and
    //     catch-block
    //
    // Additionally, a finally block may be emitted when ShouldUseControl is
    // DontUseControl, even if the kind is not TryCatchFinally or TryFinally,
    // because GOSUBs are not emitted. This internal use shares the
    // requirements as above.
    Maybe<TryFinallyControl> controlInfo_;

    int depth_;
    unsigned noteIndex_;
    ptrdiff_t tryStart_;
    JumpList catchAndFinallyJump_;
    JumpTarget tryEnd_;
    JumpTarget finallyStart_;

    enum State {
        Start,
        Try,
        TryEnd,
        Catch,
        CatchEnd,
        Finally,
        FinallyEnd,
        End
    };
    State state_;

    bool hasCatch() const {
        return kind_ == TryCatch || kind_ == TryCatchFinally;
    }
    bool hasFinally() const {
        return kind_ == TryCatchFinally || kind_ == TryFinally;
    }

  public:
    TryEmitter(BytecodeEmitter* bce, Kind kind,
               ShouldUseRetVal retValKind = UseRetVal, ShouldUseControl controlKind = UseControl);

    MOZ_MUST_USE bool emitJumpOverCatchAndFinally();

    MOZ_MUST_USE bool emitTry();
    MOZ_MUST_USE bool emitCatch();

    MOZ_MUST_USE bool emitFinally(const mozilla::Maybe<uint32_t>& finallyPos = mozilla::Nothing());

    MOZ_MUST_USE bool emitEnd();

  private:
    MOZ_MUST_USE bool emitTryEnd();
    MOZ_MUST_USE bool emitCatchEnd(bool hasNext);
    MOZ_MUST_USE bool emitFinallyEnd();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_TryEmitter_h */
