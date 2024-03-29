/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/DefaultEmitter.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "frontend/BytecodeEmitter.h"  // BytecodeEmitter
#include "vm/Opcodes.h"                // JSOP_*

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;
using mozilla::Nothing;

DefaultEmitter::DefaultEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool DefaultEmitter::prepareForDefault() {
    MOZ_ASSERT(state_ == State::Start);

    //                [stack] VALUE

    ifUndefined_.emplace(bce_);

    if (!bce_->emit1(JSOP_DUP)) {
        //              [stack] VALUE VALUE
        return false;
    }
    if (!bce_->emit1(JSOP_UNDEFINED)) {
        //              [stack] VALUE VALUE UNDEFINED
        return false;
    }
    if (!bce_->emit1(JSOP_STRICTEQ)) {
        //              [stack] VALUE EQ?
        return false;
    }

    if (!ifUndefined_->emitThen()) {
        //              [stack] VALUE
        return false;
    }

    if (!bce_->emit1(JSOP_POP)) {
        //              [stack]
        return false;
    }

#ifdef DEBUG
    state_ = State::Default;
#endif
    return true;
}

bool DefaultEmitter::emitEnd() {
    MOZ_ASSERT(state_ == State::Default);

    //                [stack] DEFAULTVALUE

    if (!ifUndefined_->emitEnd()) {
        //              [stack] VALUE/DEFAULTVALUE
        return false;
    }
    ifUndefined_.reset();

#ifdef DEBUG
    state_ = State::End;
#endif
    return true;
}
