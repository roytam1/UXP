/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS bytecode generation.
 */

#include "frontend/BytecodeEmitter.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"

#include "ds/Nestable.h"
#include "frontend/BytecodeControlStructures.h"
#include "frontend/CallOrNewEmitter.h"
#include "frontend/DefaultEmitter.h"  // DefaultEmitter
#include "frontend/ElemOpEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/ForOfLoopControl.h"
#include "frontend/FunctionEmitter.h"  // FunctionEmitter, FunctionScriptEmitter, FunctionParamsEmitter
#include "frontend/IfEmitter.h"
#include "frontend/LexicalScopeEmitter.h"  // LexicalScopeEmitter
#include "frontend/NameOpEmitter.h"
#include "frontend/ObjectEmitter.h"  // PropertyEmitter, ObjectEmitter, ClassEmitter
#include "frontend/Parser.h"
#include "frontend/PropOpEmitter.h"
#include "frontend/SwitchEmitter.h"
#include "frontend/TDZCheckCache.h"
#include "frontend/TokenStream.h"
#include "frontend/TryEmitter.h"
#include "vm/Debugger.h"
#include "vm/GeneratorObject.h"
#include "vm/Stack.h"
#include "wasm/AsmJS.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseNode-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

using mozilla::AssertedCast;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::NumberIsInt32;
using mozilla::PodCopy;
using mozilla::Some;

class OptionalEmitter;

static bool
ParseNodeRequiresSpecialLineNumberNotes(ParseNode* pn)
{
    return pn->getKind() == PNK_WHILE || pn->getKind() == PNK_FOR;
}

// Class for emitting bytecode for optional expressions.
class MOZ_RAII OptionalEmitter
{
 public:
  OptionalEmitter(BytecodeEmitter* bce, int32_t initialDepth);

 private:
  BytecodeEmitter* bce_;

  TDZCheckCache tdzCache_;

  // Jump target for short circuiting code, which has null or undefined values.
  JumpList jumpShortCircuit_;

  // Jump target for code that does not short circuit.
  JumpList jumpFinish_;

  // Stack depth when the optional emitter was instantiated.
  int32_t initialDepth_;

  // The state of this emitter.
  //
  // +-------+   emitJumpShortCircuit        +--------------+
  // | Start |-+---------------------------->| ShortCircuit |-----------+
  // +-------+ |                             +--------------+           |
  //    +----->|                                                        |
  //    |      | emitJumpShortCircuitForCall +---------------------+    v
  //    |      +---------------------------->| ShortCircuitForCall |--->+
  //    |                                    +---------------------+    |
  //    |                                                               |
  //     ---------------------------------------------------------------+
  //                                                                    |
  //                                                                    |
  // +------------------------------------------------------------------+
  // |
  // | emitOptionalJumpTarget +---------+
  // +----------------------->| JumpEnd |
  //                          +---------+
  //
#ifdef DEBUG
  enum class State {
    // The initial state.
    Start,

    // for shortcircuiting in most cases.
    ShortCircuit,

    // for shortcircuiting from references, which have two items on
    // the stack. For example function calls.
    ShortCircuitForCall,

    // internally used, end of the jump code
    JumpEnd
  };

  State state_ = State::Start;
#endif

 public:
  enum class Kind {
    // Requires two values on the stack
    Reference,
    // Requires one value on the stack
    Other
  };

  MOZ_MUST_USE bool emitJumpShortCircuit();
  MOZ_MUST_USE bool emitJumpShortCircuitForCall();

  // JSOp is the op code to be emitted, Kind is if we are dealing with a
  // reference (in which case we need two elements on the stack) or other value
  // (which needs one element on the stack)
  MOZ_MUST_USE bool emitOptionalJumpTarget(JSOp op, Kind kind = Kind::Other);
};

BytecodeEmitter::BytecodeEmitter(BytecodeEmitter* parent,
                                 Parser<FullParseHandler>* parser, SharedContext* sc,
                                 HandleScript script, Handle<LazyScript*> lazyScript,
                                 uint32_t lineNum, EmitterMode emitterMode,
                                 FieldInitializers fieldInitializers /* = FieldInitializers::Invalid() */)
  : sc(sc),
    cx(sc->context),
    parent(parent),
    script(cx, script),
    lazyScript(cx, lazyScript),
    prologue(cx, lineNum),
    main(cx, lineNum),
    current(&main),
    parser(parser),
    fieldInitializers_(fieldInitializers),
    atomIndices(cx->frontendCollectionPool()),
    firstLine(lineNum),
    maxFixedSlots(0),
    maxStackDepth(0),
    stackDepth(0),
    arrayCompDepth(0),
    emitLevel(0),
    bodyScopeIndex(UINT32_MAX),
    varEmitterScope(nullptr),
    innermostNestableControl(nullptr),
    innermostEmitterScope_(nullptr),
    innermostTDZCheckCache(nullptr),
#ifdef DEBUG
    unstableEmitterScope(false),
#endif
    constList(cx),
    scopeList(cx),
    tryNoteList(cx),
    scopeNoteList(cx),
    yieldAndAwaitOffsetList(cx),
    typesetCount(0),
    hasSingletons(false),
    hasTryFinally(false),
    emittingRunOnceLambda(false),
    emitterMode(emitterMode)
{
    MOZ_ASSERT_IF(emitterMode == LazyFunction, lazyScript);
}

BytecodeEmitter::BytecodeEmitter(BytecodeEmitter* parent,
                                 Parser<FullParseHandler>* parser, SharedContext* sc,
                                 HandleScript script, Handle<LazyScript*> lazyScript,
                                 TokenPos bodyPosition, EmitterMode emitterMode,
                                 FieldInitializers fieldInitializers)
    : BytecodeEmitter(parent, parser, sc, script, lazyScript,
                      parser->tokenStream.srcCoords.lineNum(bodyPosition.begin),
                      emitterMode, fieldInitializers)
{
    setScriptStartOffsetIfUnset(bodyPosition.begin);
    setFunctionBodyEndPos(bodyPosition.end);
}

bool
BytecodeEmitter::init()
{
    return atomIndices.acquire(cx);
}

template <typename T>
T*
BytecodeEmitter::findInnermostNestableControl() const
{
    return NestableControl::findNearest<T>(innermostNestableControl);
}

template <typename T, typename Predicate /* (T*) -> bool */>
T*
BytecodeEmitter::findInnermostNestableControl(Predicate predicate) const
{
    return NestableControl::findNearest<T>(innermostNestableControl, predicate);
}

NameLocation
BytecodeEmitter::lookupName(JSAtom* name)
{
    return innermostEmitterScope()->lookup(this, name);
}

Maybe<NameLocation>
BytecodeEmitter::locationOfNameBoundInScope(JSAtom* name, EmitterScope* target)
{
    return innermostEmitterScope()->locationBoundInScope(name, target);
}

Maybe<NameLocation>
BytecodeEmitter::locationOfNameBoundInFunctionScope(JSAtom* name, EmitterScope* source)
{
    EmitterScope* funScope = source;
    while (!funScope->scope(this)->is<FunctionScope>())
        funScope = funScope->enclosingInFrame();
    return source->locationBoundInScope(name, funScope);
}

bool
BytecodeEmitter::emitCheck(JSOp op, ptrdiff_t delta, ptrdiff_t* offset)
{
    size_t oldLength = code().length();
    *offset = ptrdiff_t(oldLength);

    size_t newLength = oldLength + size_t(delta);
    if (MOZ_UNLIKELY(newLength > MaxBytecodeLength)) {
      ReportAllocationOverflow(cx);
      return false;
    }

    if (!code().growBy(delta)) {
        ReportOutOfMemory(cx);
        return false;
    }

    // If op is JOF_TYPESET (see the type barriers comment in TypeInference.h),
    // reserve a type set to store its result.
    if (CodeSpec[op].format & JOF_TYPESET) {
        if (typesetCount < UINT16_MAX)
            typesetCount++;
    }
    return true;
}

void
BytecodeEmitter::updateDepth(ptrdiff_t target)
{
    jsbytecode* pc = code(target);

    int nuses = StackUses(nullptr, pc);
    int ndefs = StackDefs(nullptr, pc);

    stackDepth -= nuses;
    MOZ_ASSERT(stackDepth >= 0);
    stackDepth += ndefs;

    if ((uint32_t)stackDepth > maxStackDepth)
        maxStackDepth = stackDepth;
}

#ifdef DEBUG
bool
BytecodeEmitter::checkStrictOrSloppy(JSOp op)
{
    if (IsCheckStrictOp(op) && !sc->strict())
        return false;
    if (IsCheckSloppyOp(op) && sc->strict())
        return false;
    return true;
}
#endif

bool
BytecodeEmitter::emit1(JSOp op)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    ptrdiff_t offset;
    if (!emitCheck(op, 1, &offset))
        return false;

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emit2(JSOp op, uint8_t op1)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    ptrdiff_t offset;
    if (!emitCheck(op, 2, &offset))
        return false;

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    code[1] = jsbytecode(op1);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emit3(JSOp op, jsbytecode op1, jsbytecode op2)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    /* These should filter through emitVarOp. */
    MOZ_ASSERT(!IsArgOp(op));
    MOZ_ASSERT(!IsLocalOp(op));

    ptrdiff_t offset;
    if (!emitCheck(op, 3, &offset))
        return false;

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    code[1] = op1;
    code[2] = op2;
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitN(JSOp op, size_t extra, ptrdiff_t* offset)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));
    ptrdiff_t length = 1 + ptrdiff_t(extra);

    ptrdiff_t off;
    if (!emitCheck(op, length, &off))
        return false;

    jsbytecode* code = this->code(off);
    code[0] = jsbytecode(op);
    /* The remaining |extra| bytes are set by the caller */

    /*
     * Don't updateDepth if op's use-count comes from the immediate
     * operand yet to be stored in the extra bytes after op.
     */
    if (CodeSpec[op].nuses >= 0)
        updateDepth(off);

    if (offset)
        *offset = off;
    return true;
}

bool
BytecodeEmitter::emitJumpTarget(JumpTarget* target)
{
    ptrdiff_t off = offset();

    // Alias consecutive jump targets.
    if (off == current->lastTarget.offset + ptrdiff_t(JSOP_JUMPTARGET_LENGTH)) {
        target->offset = current->lastTarget.offset;
        return true;
    }

    target->offset = off;
    current->lastTarget.offset = off;
    if (!emit1(JSOP_JUMPTARGET))
        return false;
    return true;
}

bool
BytecodeEmitter::emitJumpNoFallthrough(JSOp op, JumpList* jump)
{
    ptrdiff_t offset;
    if (!emitCheck(op, 5, &offset))
        return false;

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    MOZ_ASSERT(-1 <= jump->offset && jump->offset < offset);
    jump->push(this->code(0), offset);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitJump(JSOp op, JumpList* jump)
{
    if (!emitJumpNoFallthrough(op, jump))
        return false;
    if (BytecodeFallsThrough(op)) {
        JumpTarget fallthrough;
        if (!emitJumpTarget(&fallthrough))
            return false;
    }
    return true;
}

bool
BytecodeEmitter::emitBackwardJump(JSOp op, JumpTarget target, JumpList* jump, JumpTarget* fallthrough)
{
    if (!emitJumpNoFallthrough(op, jump))
        return false;
    patchJumpsToTarget(*jump, target);

    // Unconditionally create a fallthrough for closing iterators, and as a
    // target for break statements.
    if (!emitJumpTarget(fallthrough))
        return false;
    return true;
}

void
BytecodeEmitter::patchJumpsToTarget(JumpList jump, JumpTarget target)
{
    MOZ_ASSERT(-1 <= jump.offset && jump.offset <= offset());
    MOZ_ASSERT(0 <= target.offset && target.offset <= offset());
    MOZ_ASSERT_IF(jump.offset != -1 && target.offset + 4 <= offset(),
                  BytecodeIsJumpTarget(JSOp(*code(target.offset))));
    jump.patchAll(code(0), target);
}

bool
BytecodeEmitter::emitJumpTargetAndPatch(JumpList jump)
{
    if (jump.offset == -1)
        return true;
    JumpTarget target;
    if (!emitJumpTarget(&target))
        return false;
    patchJumpsToTarget(jump, target);
    return true;
}

bool
BytecodeEmitter::emitCall(JSOp op, uint16_t argc, const Maybe<uint32_t>& sourceCoordOffset)
{
    if (sourceCoordOffset.isSome()) {
        if (!updateSourceCoordNotes(*sourceCoordOffset))
            return false;
    }
    return emit3(op, ARGC_HI(argc), ARGC_LO(argc));
}

bool
BytecodeEmitter::emitCall(JSOp op, uint16_t argc, ParseNode* pn)
{
    if (pn && !updateSourceCoordNotes(pn->pn_pos.begin))
        return false;
    return emitCall(op, argc, pn ? Some(pn->pn_pos.begin) : Nothing());
}

bool
BytecodeEmitter::emitDupAt(unsigned slotFromTop)
{
    MOZ_ASSERT(slotFromTop < unsigned(stackDepth));

    if (slotFromTop >= JS_BIT(24)) {
        reportError(nullptr, JSMSG_TOO_MANY_LOCALS);
        return false;
    }

    ptrdiff_t off;
    if (!emitN(JSOP_DUPAT, 3, &off))
        return false;

    jsbytecode* pc = code(off);
    SET_UINT24(pc, slotFromTop);
    return true;
}

bool
BytecodeEmitter::emitPopN(unsigned n)
{
    MOZ_ASSERT(n != 0);

    if (n == 1)
        return emit1(JSOP_POP);

    // 2 JSOP_POPs (2 bytes) are shorter than JSOP_POPN (3 bytes).
    if (n == 2)
        return emit1(JSOP_POP) && emit1(JSOP_POP);

    return emitUint16Operand(JSOP_POPN, n);
}

bool
BytecodeEmitter::emitUnpickN(unsigned n)
{
    MOZ_ASSERT(n != 0);

    if (n == 1)
        return emit1(JSOP_SWAP);

    return emit2(JSOP_UNPICK, n);
}

bool
BytecodeEmitter::emitCheckIsObj(CheckIsObjectKind kind)
{
    return emit2(JSOP_CHECKISOBJ, uint8_t(kind));
}

bool
BytecodeEmitter::emitCheckIsCallable(CheckIsCallableKind kind)
{
    return emit2(JSOP_CHECKISCALLABLE, uint8_t(kind));
}

static inline unsigned
LengthOfSetLine(unsigned line)
{
    return 1 /* SN_SETLINE */ + (line > SN_4BYTE_OFFSET_MASK ? 4 : 1);
}

/* Updates line number notes, not column notes. */
bool
BytecodeEmitter::updateLineNumberNotes(uint32_t offset)
{
    TokenStream* ts = &parser->tokenStream;
    bool onThisLine;
    if (!ts->srcCoords.isOnThisLine(offset, currentLine(), &onThisLine))
        return ts->reportError(JSMSG_OUT_OF_MEMORY);
    if (!onThisLine) {
        unsigned line = ts->srcCoords.lineNum(offset);
        unsigned delta = line - currentLine();

        /*
         * Encode any change in the current source line number by using
         * either several SRC_NEWLINE notes or just one SRC_SETLINE note,
         * whichever consumes less space.
         *
         * NB: We handle backward line number deltas (possible with for
         * loops where the update part is emitted after the body, but its
         * line number is <= any line number in the body) here by letting
         * unsigned delta_ wrap to a very large number, which triggers a
         * SRC_SETLINE.
         */
        current->currentLine = line;
        current->lastColumn  = 0;
        if (delta >= LengthOfSetLine(line)) {
            if (!newSrcNote2(SRC_SETLINE, ptrdiff_t(line)))
                return false;
        } else {
            do {
                if (!newSrcNote(SRC_NEWLINE))
                    return false;
            } while (--delta != 0);
        }
    }
    return true;
}

/* Updates the line number and column number information in the source notes. */
bool
BytecodeEmitter::updateSourceCoordNotes(uint32_t offset)
{
    if (!updateLineNumberNotes(offset))
        return false;

    uint32_t columnIndex = parser->tokenStream.srcCoords.columnIndex(offset);
    ptrdiff_t colspan = ptrdiff_t(columnIndex) - ptrdiff_t(current->lastColumn);
    if (colspan != 0) {
        // If the column span is so large that we can't store it, then just
        // discard this information. This can happen with minimized or otherwise
        // machine-generated code. Even gigantic column numbers are still
        // valuable if you have a source map to relate them to something real;
        // but it's better to fail soft here.
        if (!SN_REPRESENTABLE_COLSPAN(colspan))
            return true;
        if (!newSrcNote2(SRC_COLSPAN, SN_COLSPAN_TO_OFFSET(colspan)))
            return false;
        current->lastColumn = columnIndex;
    }
    return true;
}

bool
BytecodeEmitter::emitLoopHead(ParseNode* nextpn, JumpTarget* top)
{
    if (nextpn) {
        /*
         * Try to give the JSOP_LOOPHEAD the same line number as the next
         * instruction. nextpn is often a block, in which case the next
         * instruction typically comes from the first statement inside.
         */
        if (nextpn->is<LexicalScopeNode>())
            nextpn = nextpn->as<LexicalScopeNode>().scopeBody();
        if (nextpn->isKind(PNK_STATEMENTLIST)) {
            if (ParseNode* firstStatement = nextpn->as<ListNode>().head()) {
                nextpn = firstStatement;
            }
        }
        if (!updateSourceCoordNotes(nextpn->pn_pos.begin))
            return false;
    }

    *top = { offset() };
    return emit1(JSOP_LOOPHEAD);
}

bool
BytecodeEmitter::emitLoopEntry(ParseNode* nextpn, JumpList entryJump)
{
    if (nextpn) {
        /* Update the line number, as for LOOPHEAD. */
        if (nextpn->is<LexicalScopeNode>())
            nextpn = nextpn->as<LexicalScopeNode>().scopeBody();
        if (nextpn->isKind(PNK_STATEMENTLIST)) {
            if (ParseNode* firstStatement = nextpn->as<ListNode>().head()) {
                nextpn = firstStatement;
            }
        }
        if (!updateSourceCoordNotes(nextpn->pn_pos.begin))
            return false;
    }

    JumpTarget entry{ offset() };
    patchJumpsToTarget(entryJump, entry);

    LoopControl& loopInfo = innermostNestableControl->as<LoopControl>();
    MOZ_ASSERT(loopInfo.loopDepth() > 0);

    uint8_t loopDepthAndFlags = PackLoopEntryDepthHintAndFlags(loopInfo.loopDepth(),
                                                               loopInfo.canIonOsr());
    return emit2(JSOP_LOOPENTRY, loopDepthAndFlags);
}

bool
BytecodeEmitter::emitUint16Operand(JSOp op, uint32_t operand)
{
    MOZ_ASSERT(operand <= UINT16_MAX);
    if (!emit3(op, UINT16_HI(operand), UINT16_LO(operand)))
        return false;
    return true;
}

bool
BytecodeEmitter::emitUint32Operand(JSOp op, uint32_t operand)
{
    ptrdiff_t off;
    if (!emitN(op, 4, &off))
        return false;
    SET_UINT32(code(off), operand);
    return true;
}

namespace {

class NonLocalExitControl
{
  public:
    enum Kind
    {
        // IteratorClose is handled especially inside the exception unwinder.
        Throw,

        // A 'continue' statement does not call IteratorClose for the loop it
        // is continuing, i.e. excluding the target loop.
        Continue,

        // A 'break' or 'return' statement does call IteratorClose for the
        // loop it is breaking out of or returning from, i.e. including the
        // target loop.
        Break,
        Return
    };

  private:
    BytecodeEmitter* bce_;
    const uint32_t savedScopeNoteIndex_;
    const int savedDepth_;
    uint32_t openScopeNoteIndex_;
    Kind kind_;

    NonLocalExitControl(const NonLocalExitControl&) = delete;

    MOZ_MUST_USE bool leaveScope(EmitterScope* scope);

  public:
    NonLocalExitControl(BytecodeEmitter* bce, Kind kind)
      : bce_(bce),
        savedScopeNoteIndex_(bce->scopeNoteList.length()),
        savedDepth_(bce->stackDepth),
        openScopeNoteIndex_(bce->innermostEmitterScope()->noteIndex()),
        kind_(kind)
    { }

    ~NonLocalExitControl() {
        for (uint32_t n = savedScopeNoteIndex_; n < bce_->scopeNoteList.length(); n++)
            bce_->scopeNoteList.recordEnd(n, bce_->offset(), bce_->inPrologue());
        bce_->stackDepth = savedDepth_;
    }

    MOZ_MUST_USE bool prepareForNonLocalJump(NestableControl* target);

    MOZ_MUST_USE bool prepareForNonLocalJumpToOutermost() {
        return prepareForNonLocalJump(nullptr);
    }
};

bool
NonLocalExitControl::leaveScope(EmitterScope* es)
{
    if (!es->leave(bce_, /* nonLocal = */ true))
        return false;

    // As we pop each scope due to the non-local jump, emit notes that
    // record the extent of the enclosing scope. These notes will have
    // their ends recorded in ~NonLocalExitControl().
    uint32_t enclosingScopeIndex = ScopeNote::NoScopeIndex;
    if (es->enclosingInFrame())
        enclosingScopeIndex = es->enclosingInFrame()->index();
    if (!bce_->scopeNoteList.append(enclosingScopeIndex, bce_->offset(), bce_->inPrologue(),
                                    openScopeNoteIndex_))
        return false;
    openScopeNoteIndex_ = bce_->scopeNoteList.length() - 1;

    return true;
}

/*
 * Emit additional bytecode(s) for non-local jumps.
 */
bool
NonLocalExitControl::prepareForNonLocalJump(NestableControl* target)
{
    EmitterScope* es = bce_->innermostEmitterScope();
    int npops = 0;

    AutoCheckUnstableEmitterScope cues(bce_);

    // For 'continue', 'break', and 'return' statements, emit IteratorClose
    // bytecode inline. 'continue' statements do not call IteratorClose for
    // the loop they are continuing.
    bool emitIteratorClose = kind_ == Continue || kind_ == Break || kind_ == Return;
    bool emitIteratorCloseAtTarget = emitIteratorClose && kind_ != Continue;

    auto flushPops = [&npops](BytecodeEmitter* bce) {
        if (npops && !bce->emitUint16Operand(JSOP_POPN, npops))
            return false;
        npops = 0;
        return true;
    };

    // Walk the nestable control stack and patch jumps.
    for (NestableControl* control = bce_->innermostNestableControl;
         control != target;
         control = control->enclosing())
    {
        // Walk the scope stack and leave the scopes we entered. Leaving a scope
        // may emit administrative ops like JSOP_POPLEXICALENV but never anything
        // that manipulates the stack.
        for (; es != control->emitterScope(); es = es->enclosingInFrame()) {
            if (!leaveScope(es))
                return false;
        }

        switch (control->kind()) {
          case StatementKind::Finally: {
            TryFinallyControl& finallyControl = control->as<TryFinallyControl>();
            if (finallyControl.emittingSubroutine()) {
                /*
                 * There's a [exception or hole, retsub pc-index] pair and the
                 * possible return value on the stack that we need to pop.
                 */
                npops += 3;
            } else {
                if (!flushPops(bce_))
                    return false;
                if (!bce_->emitJump(JSOP_GOSUB, &finallyControl.gosubs)) // ...
                    return false;
            }
            break;
          }

          case StatementKind::ForOfLoop:
            if (emitIteratorClose) {
                if (!flushPops(bce_))
                    return false;

                ForOfLoopControl& loopinfo = control->as<ForOfLoopControl>();
                if (!loopinfo.emitPrepareForNonLocalJumpFromScope(bce_, *es,
                                                                  /* isTarget = */ false))
                {                                         // ...
                    return false;
                }
            } else {
                npops += 3;
            }
            break;

          case StatementKind::ForInLoop:
            if (!flushPops(bce_))
                return false;

            // The iterator and the current value are on the stack.
            if (!bce_->emit1(JSOP_POP))                   // ... ITER
                return false;
            if (!bce_->emit1(JSOP_ENDITER))               // ...
                return false;
            break;

          default:
            break;
        }
    }

    if (!flushPops(bce_))
        return false;

    if (target && emitIteratorCloseAtTarget && target->is<ForOfLoopControl>()) {
        ForOfLoopControl& loopinfo = target->as<ForOfLoopControl>();
        if (!loopinfo.emitPrepareForNonLocalJumpFromScope(bce_, *es,
                                                          /* isTarget = */ true))
        {                                                 // ... UNDEF UNDEF UNDEF
            return false;
        }
    }

    EmitterScope* targetEmitterScope = target ? target->emitterScope() : bce_->varEmitterScope;
    for (; es != targetEmitterScope; es = es->enclosingInFrame()) {
        if (!leaveScope(es))
            return false;
    }

    return true;
}

}  // anonymous namespace

bool
BytecodeEmitter::emitGoto(NestableControl* target, JumpList* jumplist, SrcNoteType noteType)
{
    NonLocalExitControl nle(this, noteType == SRC_CONTINUE
                                  ? NonLocalExitControl::Continue
                                  : NonLocalExitControl::Break);

    if (!nle.prepareForNonLocalJump(target))
        return false;

    if (noteType != SRC_NULL) {
        if (!newSrcNote(noteType))
            return false;
    }

    return emitJump(JSOP_GOTO, jumplist);
}

Scope*
BytecodeEmitter::innermostScope() const
{
    return innermostEmitterScope()->scope(this);
}

bool
BytecodeEmitter::emitIndex32(JSOp op, uint32_t index)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    const size_t len = 1 + UINT32_INDEX_LEN;
    MOZ_ASSERT(len == size_t(CodeSpec[op].length));

    ptrdiff_t offset;
    if (!emitCheck(op, len, &offset))
        return false;

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    SET_UINT32_INDEX(code, index);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitIndexOp(JSOp op, uint32_t index)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    const size_t len = CodeSpec[op].length;
    MOZ_ASSERT(len >= 1 + UINT32_INDEX_LEN);

    ptrdiff_t offset;
    if (!emitCheck(op, len, &offset))
        return false;

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    SET_UINT32_INDEX(code, index);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitAtomOp(JSAtom* atom, JSOp op)
{
    MOZ_ASSERT(atom);

    // .generator lookups should be emitted as JSOP_GETALIASEDVAR instead of
    // JSOP_GETNAME etc, to bypass |with| objects on the scope chain.
    // It's safe to emit .this lookups though because |with| objects skip
    // those.
    MOZ_ASSERT_IF(op == JSOP_GETNAME || op == JSOP_GETGNAME,
                  atom != cx->names().dotGenerator);

    if (op == JSOP_GETPROP && atom == cx->names().length) {
        /* Specialize length accesses for the interpreter. */
        op = JSOP_LENGTH;
    }

    uint32_t index;
    if (!makeAtomIndex(atom, &index))
        return false;

    return emitAtomOp(index, op);
}

bool
BytecodeEmitter::emitAtomOp(uint32_t atomIndex, JSOp op)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);

    return emitIndexOp(op, atomIndex);
}

bool
BytecodeEmitter::emitInternedScopeOp(uint32_t index, JSOp op)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_SCOPE);
    MOZ_ASSERT(index < scopeList.length());
    return emitIndex32(op, index);
}

bool
BytecodeEmitter::emitInternedObjectOp(uint32_t index, JSOp op)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_OBJECT);
    MOZ_ASSERT(index < objectList.length);
    return emitIndex32(op, index);
}

bool
BytecodeEmitter::emitObjectOp(ObjectBox* objbox, JSOp op)
{
    return emitInternedObjectOp(objectList.add(objbox), op);
}

bool
BytecodeEmitter::emitObjectPairOp(ObjectBox* objbox1, ObjectBox* objbox2, JSOp op)
{
    uint32_t index = objectList.add(objbox1);
    objectList.add(objbox2);
    return emitInternedObjectOp(index, op);
}

bool
BytecodeEmitter::emitRegExp(uint32_t index)
{
    return emitIndex32(JSOP_REGEXP, index);
}

bool
BytecodeEmitter::emitLocalOp(JSOp op, uint32_t slot)
{
    MOZ_ASSERT(JOF_OPTYPE(op) != JOF_ENVCOORD);
    MOZ_ASSERT(IsLocalOp(op));

    ptrdiff_t off;
    if (!emitN(op, LOCALNO_LEN, &off))
        return false;

    SET_LOCALNO(code(off), slot);
    return true;
}

bool
BytecodeEmitter::emitArgOp(JSOp op, uint16_t slot)
{
    MOZ_ASSERT(IsArgOp(op));
    ptrdiff_t off;
    if (!emitN(op, ARGNO_LEN, &off))
        return false;

    SET_ARGNO(code(off), slot);
    return true;
}

bool
BytecodeEmitter::emitEnvCoordOp(JSOp op, EnvironmentCoordinate ec)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_ENVCOORD);

    unsigned n = ENVCOORD_HOPS_LEN + ENVCOORD_SLOT_LEN;
    MOZ_ASSERT(int(n) + 1 /* op */ == CodeSpec[op].length);

    ptrdiff_t off;
    if (!emitN(op, n, &off))
        return false;

    jsbytecode* pc = code(off);
    SET_ENVCOORD_HOPS(pc, ec.hops());
    pc += ENVCOORD_HOPS_LEN;
    SET_ENVCOORD_SLOT(pc, ec.slot());
    pc += ENVCOORD_SLOT_LEN;
    return true;
}

JSOp
BytecodeEmitter::strictifySetNameOp(JSOp op)
{
    switch (op) {
      case JSOP_SETNAME:
        if (sc->strict())
            op = JSOP_STRICTSETNAME;
        break;
      case JSOP_SETGNAME:
        if (sc->strict())
            op = JSOP_STRICTSETGNAME;
        break;
        default:;
    }
    return op;
}

bool
BytecodeEmitter::checkSideEffects(ParseNode* pn, bool* answer)
{
    JS_CHECK_RECURSION(cx, return false);

 restart:

    switch (pn->getKind()) {
      // Trivial cases with no side effects.
      case PNK_NOP:
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_NULL:
      case PNK_RAW_UNDEFINED:
      case PNK_ELISION:
      case PNK_GENERATOR:
        MOZ_ASSERT(pn->is<NullaryNode>());
        *answer = false;
        return true;

      case PNK_OBJECT_PROPERTY_NAME:
      case PNK_PRIVATE_NAME:                               // no side effects, unlike PNK_NAME
      case PNK_STRING:
      case PNK_TEMPLATE_STRING:
        MOZ_ASSERT(pn->is<NameNode>());
        *answer = false;
        return true;

      case PNK_REGEXP:
        MOZ_ASSERT(pn->is<RegExpLiteral>());
        *answer = false;
        return true;

      case PNK_NUMBER:
        MOZ_ASSERT(pn->is<NumericLiteral>());
        *answer = false;
        return true;

      case PNK_BIGINT:
        MOZ_ASSERT(pn->is<BigIntLiteral>());
        *answer = false;
        return true;

      // |this| can throw in derived class constructors, including nested arrow
      // functions or eval.
      case PNK_THIS:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = sc->needsThisTDZChecks();
        return true;

      // Trivial binary nodes with more token pos holders.
      case PNK_NEWTARGET:
      case PNK_IMPORT_META:
        MOZ_ASSERT(pn->as<BinaryNode>().left()->isKind(PNK_POSHOLDER));
        MOZ_ASSERT(pn->as<BinaryNode>().right()->isKind(PNK_POSHOLDER));
        *answer = false;
        return true;

      case PNK_BREAK:
        MOZ_ASSERT(pn->is<BreakStatement>());
        *answer = true;
        return true;

      case PNK_CONTINUE:
        MOZ_ASSERT(pn->is<ContinueStatement>());
        *answer = true;
        return true;

      case PNK_DEBUGGER:
        MOZ_ASSERT(pn->is<DebuggerStatement>());
        *answer = true;
        return true;

      // Watch out for getters!
      case PNK_DOT:
      case PNK_OPTDOT:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Unary cases with side effects only if the child has them.
      case PNK_TYPEOFEXPR:
      case PNK_VOID:
      case PNK_NOT:
        return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

      // Even if the name expression is effect-free, performing ToPropertyKey on
      // it might not be effect-free:
      //
      //   RegExp.prototype.toString = () => { throw 42; };
      //   ({ [/regex/]: 0 }); // ToPropertyKey(/regex/) throws 42
      //
      //   function Q() {
      //     ({ [new.target]: 0 });
      //   }
      //   Q.toString = () => { throw 17; };
      //   new Q; // new.target will be Q, ToPropertyKey(Q) throws 17
      case PNK_COMPUTED_NAME:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // Looking up or evaluating the associated name could throw.
      case PNK_TYPEOFNAME:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // These unary cases have side effects on the enclosing object/array,
      // sure.  But that's not the question this function answers: it's
      // whether the operation may have a side effect on something *other* than
      // the result of the overall operation in which it's embedded.  The
      // answer to that is no, for an object literal having a mutated prototype
      // and an array comprehension containing no other effectful operations
      // only produce a value, without affecting anything else.
      case PNK_MUTATEPROTO:
      case PNK_ARRAYPUSH:
        return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

      // Unary cases with obvious side effects.
      case PNK_PREINCREMENT:
      case PNK_POSTINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTDECREMENT:
      case PNK_THROW:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // These might invoke valueOf/toString, even with a subexpression without
      // side effects!  Consider |+{ valueOf: null, toString: null }|.
      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // This invokes the (user-controllable) iterator protocol.
      case PNK_SPREAD:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      case PNK_INITIALYIELD:
      case PNK_YIELD_STAR:
      case PNK_YIELD:
      case PNK_AWAIT:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // Deletion generally has side effects, even if isolated cases have none.
      case PNK_DELETENAME:
      case PNK_DELETEPROP:
      case PNK_DELETEELEM:
      case PNK_DELETEOPTCHAIN:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // Deletion of a non-Reference expression has side effects only through
      // evaluating the expression.
      case PNK_DELETEEXPR: {
        ParseNode* expr = pn->as<UnaryNode>().kid();
        return checkSideEffects(expr, answer);
      }

      case PNK_SEMI:
        if (ParseNode* expr = pn->as<UnaryNode>().kid())
            return checkSideEffects(expr, answer);
        *answer = false;
        return true;

      // Binary cases with obvious side effects.
      case PNK_INITPROP:
        *answer = true;
        return true;

      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
      case PNK_SUBASSIGN:
      case PNK_COALESCEASSIGN:
      case PNK_ORASSIGN:
      case PNK_ANDASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_MULASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
      case PNK_POWASSIGN:
        MOZ_ASSERT(pn->is<AssignmentNode>());
        *answer = true;
        return true;

      case PNK_SETTHIS:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case PNK_STATEMENTLIST:
      case PNK_CATCHLIST:
      // Strict equality operations and logical operators are well-behaved and
      // perform no conversions.
      case PNK_COALESCE:
      case PNK_OR:
      case PNK_AND:
      case PNK_STRICTEQ:
      case PNK_STRICTNE:
      // Any subexpression of a comma expression could be effectful.
      case PNK_COMMA:
        MOZ_ASSERT(!pn->as<ListNode>().empty());
        MOZ_FALLTHROUGH;
      // Subcomponents of a literal may be effectful.
      case PNK_ARRAY:
      case PNK_OBJECT:
        for (ParseNode* item : pn->as<ListNode>().contents()) {
            if (!checkSideEffects(item, answer))
                return false;
            if (*answer)
                return true;
        }
        return true;

      // Most other binary operations (parsed as lists in SpiderMonkey) may
      // perform conversions triggering side effects.  Math operations perform
      // ToNumber and may fail invoking invalid user-defined toString/valueOf:
      // |5 < { toString: null }|.  |instanceof| throws if provided a
      // non-object constructor: |null instanceof null|.  |in| throws if given
      // a non-object RHS: |5 in null|.
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_EQ:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_INSTANCEOF:
      case PNK_IN:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_ADD:
      case PNK_SUB:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
      case PNK_POW:
        MOZ_ASSERT(pn->as<ListNode>().count() >= 2);
        *answer = true;
        return true;

      case PNK_COLON:
      case PNK_CASE: {
        BinaryNode* node = &pn->as<BinaryNode>();
        if (!checkSideEffects(node->left(), answer))
            return false;
        if (*answer)
            return true;
        return checkSideEffects(node->right(), answer);
      }

      // More getters.
      case PNK_ELEM:
      case PNK_OPTELEM:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // These affect visible names in this code, or in other code.
      case PNK_IMPORT:
      case PNK_EXPORT_FROM:
      case PNK_EXPORT_DEFAULT:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Likewise.
      case PNK_EXPORT:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      case PNK_CALL_IMPORT:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Every part of a loop might be effect-free, but looping infinitely *is*
      // an effect.  (Language lawyer trivia: C++ says threads can be assumed
      // to exit or have side effects, C++14 [intro.multithread]p27, so a C++
      // implementation's equivalent of the below could set |*answer = false;|
      // if all loop sub-nodes set |*answer = false|!)
      case PNK_DOWHILE:
      case PNK_WHILE:
      case PNK_FOR:
      case PNK_COMPREHENSIONFOR:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Declarations affect the name set of the relevant scope.
      case PNK_VAR:
      case PNK_CONST:
      case PNK_LET:
        MOZ_ASSERT(pn->is<ListNode>());
        *answer = true;
        return true;

      case PNK_IF:
      case PNK_CONDITIONAL:
      {
        TernaryNode* node = &pn->as<TernaryNode>();
        if (!checkSideEffects(node->kid1(), answer))
            return false;
        if (*answer)
            return true;
        if (!checkSideEffects(node->kid2(), answer))
            return false;
        if (*answer)
            return true;
        if ((pn = node->kid3()))
            goto restart;
        return true;
      }

      // Function calls can invoke non-local code.
      case PNK_NEW:
      case PNK_CALL:
      case PNK_OPTCALL:
      case PNK_TAGGED_TEMPLATE:
      case PNK_SUPERCALL:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Function arg lists can contain arbitrary expressions. Technically
      // this only causes side-effects if one of the arguments does, but since
      // the call being made will always trigger side-effects, it isn't needed.
      case PNK_ARGUMENTS:
        MOZ_ASSERT(pn->is<ListNode>());
        *answer = true;
        return true;

      case PNK_OPTCHAIN:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // Classes typically introduce names.  Even if no name is introduced,
      // the heritage and/or class body (through computed property names)
      // usually have effects.
      case PNK_CLASS:
        MOZ_ASSERT(pn->is<ClassNode>());
        *answer = true;
        return true;

      // |with| calls |ToObject| on its expression and so throws if that value
      // is null/undefined.
      case PNK_WITH:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case PNK_RETURN:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case PNK_NAME:
        MOZ_ASSERT(pn->is<NameNode>());
        *answer = true;
        return true;

      // Shorthands could trigger getters: the |x| in the object literal in
      // |with ({ get x() { throw 42; } }) ({ x });|, for example, triggers
      // one.  (Of course, it isn't necessary to use |with| for a shorthand to
      // trigger a getter.)
      case PNK_SHORTHAND:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case PNK_FUNCTION:
        MOZ_ASSERT(pn->is<FunctionNode>());
        /*
         * A named function, contrary to ES3, is no longer effectful, because
         * we bind its name lexically (using JSOP_CALLEE) instead of creating
         * an Object instance and binding a readonly, permanent property in it
         * (the object and binding can be detected and hijacked or captured).
         * This is a bug fix to ES3; it is fixed in ES3.1 drafts.
         */
        *answer = false;
        return true;

      case PNK_MODULE:
        *answer = false;
        return true;

      // Generator expressions have no side effects on their own.
      case PNK_GENEXP:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = false;
        return true;

      case PNK_TRY:
      {
        TryNode* tryNode = &pn->as<TryNode>();
        if (!checkSideEffects(tryNode->body(), answer))
            return false;
        if (*answer)
            return true;
        if (ListNode* catchList = tryNode->catchList()) {
            if (!checkSideEffects(catchList, answer))
                return false;
            if (*answer)
                return true;
        }
        if (ParseNode* finallyBlock = tryNode->finallyBlock()) {
            if (!checkSideEffects(finallyBlock, answer))
                return false;
        }
        return true;
      }

      case PNK_CATCH:
      {
        TernaryNode* catchNode = &pn->as<TernaryNode>();
        if (ParseNode* binding = catchNode->kid1()) {
            if (!checkSideEffects(binding, answer))
                return false;
            if (*answer)
                return true;
        }
        if (ParseNode* cond = catchNode->kid2()) {
            if (!checkSideEffects(cond, answer))
                return false;
            if (*answer)
                return true;
        }
        return checkSideEffects(catchNode->kid3(), answer);
      }

      case PNK_SWITCH: {
        SwitchStatement* switchStmt = &pn->as<SwitchStatement>();
        if (!checkSideEffects(&switchStmt->discriminant(), answer))
            return false;
        return *answer || checkSideEffects(&switchStmt->lexicalForCaseList(), answer);
      }

      case PNK_LABEL:
        return checkSideEffects(pn->as<LabeledStatement>().statement(), answer);

      case PNK_LEXICALSCOPE:
        MOZ_ASSERT(pn->isArity(PN_SCOPE));
        return checkSideEffects(pn->as<LexicalScopeNode>().scopeBody(), answer);

      // We could methodically check every interpolated expression, but it's
      // probably not worth the trouble.  Treat template strings as effect-free
      // only if they don't contain any substitutions.
      case PNK_TEMPLATE_STRING_LIST: {
        ListNode* list = &pn->as<ListNode>();
        MOZ_ASSERT(!list->empty());
        MOZ_ASSERT((list->count() % 2) == 1,
                   "template strings must alternate template and substitution "
                   "parts");
        *answer = list->count() > 1;
        return true;
      }

      case PNK_ARRAYCOMP: {
        ListNode* list = &pn->as<ListNode>();
        MOZ_ASSERT(list->count() == 1);
        return checkSideEffects(list->head(), answer);
      }

      // This should be unreachable but is left as-is for now.
      case PNK_PARAMSBODY:
        *answer = true;
        return true;

      case PNK_FORIN:           // by PNK_FOR/PNK_COMPREHENSIONFOR
      case PNK_FOROF:           // by PNK_FOR/PNK_COMPREHENSIONFOR
      case PNK_FORHEAD:         // by PNK_FOR/PNK_COMPREHENSIONFOR
      case PNK_CLASSMETHOD:     // by PNK_CLASS
      case PNK_CLASSFIELD:      // by PNK_CLASS
      case PNK_CLASSNAMES:      // by PNK_CLASS
      case PNK_STATICCLASSBLOCK:// by PNK_CLASS
      case PNK_CLASSMEMBERLIST: // by PNK_CLASS
      case PNK_IMPORT_SPEC_LIST: // by PNK_IMPORT
      case PNK_IMPORT_SPEC:      // by PNK_IMPORT
      case PNK_EXPORT_BATCH_SPEC:// by PNK_EXPORT
      case PNK_EXPORT_SPEC_LIST: // by PNK_EXPORT
      case PNK_EXPORT_SPEC:      // by PNK_EXPORT
      case PNK_CALLSITEOBJ:      // by PNK_TAGGED_TEMPLATE
      case PNK_POSHOLDER:        // by PNK_NEWTARGET
      case PNK_SUPERBASE:        // by PNK_ELEM and others
      case PNK_PROPERTYNAME:     // by PNK_DOT
        MOZ_CRASH("handled by parent nodes");

      case PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("invalid node kind");
    }

    MOZ_CRASH("invalid, unenumerated ParseNodeKind value encountered in "
              "BytecodeEmitter::checkSideEffects");
}

bool
BytecodeEmitter::isInLoop()
{
    return findInnermostNestableControl<LoopControl>();
}

bool
BytecodeEmitter::checkSingletonContext()
{
    if (!script->treatAsRunOnce() || sc->isFunctionBox() || isInLoop())
        return false;
    hasSingletons = true;
    return true;
}

bool
BytecodeEmitter::checkRunOnceContext()
{
    return checkSingletonContext() || (!isInLoop() && isRunOnceLambda());
}

bool
BytecodeEmitter::needsImplicitThis()
{
    // Short-circuit if there is an enclosing 'with' scope.
    if (sc->inWith())
        return true;

    // Otherwise see if the current point is under a 'with'.
    for (EmitterScope* es = innermostEmitterScope(); es; es = es->enclosingInFrame()) {
        if (es->scope(this)->kind() == ScopeKind::With)
            return true;
    }

    return false;
}

bool
BytecodeEmitter::maybeSetDisplayURL()
{
    if (tokenStream().hasDisplayURL()) {
        if (!parser->ss->setDisplayURL(cx, tokenStream().displayURL())) {
            return false;
        }
    }
    return true;
}

bool
BytecodeEmitter::maybeSetSourceMap()
{
    if (tokenStream().hasSourceMapURL()) {
        MOZ_ASSERT(!parser->ss->hasSourceMapURL());
        if (!parser->ss->setSourceMapURL(cx, tokenStream().sourceMapURL())) {
            return false;
        }
    }

    /*
     * Source map URLs passed as a compile option (usually via a HTTP source map
     * header) override any source map urls passed as comment pragmas.
     */
    if (parser->options().sourceMapURL()) {
        // Warn about the replacement, but use the new one.
        if (parser->ss->hasSourceMapURL()) {
            if (!parser->reportNoOffset(ParseWarning, false, JSMSG_ALREADY_HAS_PRAGMA,
                                        parser->ss->filename(), "//# sourceMappingURL"))
            {
                return false;
            }
        }

        if (!parser->ss->setSourceMapURL(cx, parser->options().sourceMapURL()))
            return false;
    }

    return true;
}

void
BytecodeEmitter::tellDebuggerAboutCompiledScript(ExclusiveContext* cx)
{
    // Note: when parsing off thread the resulting scripts need to be handed to
    // the debugger after rejoining to the main thread.
    if (!cx->isJSContext())
        return;

    // Lazy scripts are never top level (despite always being invoked with a
    // nullptr parent), and so the hook should never be fired.
    if (emitterMode != LazyFunction && !parent) {
        Debugger::onNewScript(cx->asJSContext(), script);
    }
}

inline TokenStream&
BytecodeEmitter::tokenStream()
{
    return parser->tokenStream;
}

bool
BytecodeEmitter::reportError(ParseNode* pn, unsigned errorNumber, ...)
{
    uint32_t offset = pn ? pn->pn_pos.begin : *scriptStartOffset;

    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream().reportCompileErrorNumberVA(nullptr, offset, JSREPORT_ERROR,
                                                           errorNumber, args);
    va_end(args);
    return result;
}

bool
BytecodeEmitter::reportError(const mozilla::Maybe<uint32_t>& maybeOffset, unsigned errorNumber, ...)
{
    uint32_t offset = maybeOffset ? *maybeOffset : *scriptStartOffset;

    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream().reportCompileErrorNumberVA(nullptr, offset, JSREPORT_ERROR,
                                                           errorNumber, args);
    va_end(args);
    return result;
}

bool
BytecodeEmitter::reportExtraWarning(ParseNode* pn, unsigned errorNumber, ...)
{
    uint32_t offset = pn ? pn->pn_pos.begin : *scriptStartOffset;

    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream().reportExtraWarningErrorNumberVA(nullptr, offset,
                                                                errorNumber, args);
    va_end(args);
    return result;
}

bool
BytecodeEmitter::reportExtraWarning(const mozilla::Maybe<uint32_t>& maybeOffset, unsigned errorNumber, ...)
{
    uint32_t offset = maybeOffset ? *maybeOffset : tokenStream().currentToken().pos.begin;

    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream().reportExtraWarningErrorNumberVA(nullptr, offset,
                                                                errorNumber, args);
    va_end(args);
    return result;
}

bool
BytecodeEmitter::reportStrictModeError(ParseNode* pn, unsigned errorNumber, ...)
{
    TokenPos pos = pn ? pn->pn_pos : tokenStream().currentToken().pos;

    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream().reportStrictModeErrorNumberVA(nullptr, pos.begin, sc->strict(),
                                                              errorNumber, args);
    va_end(args);
    return result;
}

bool
BytecodeEmitter::emitNewInit(JSProtoKey key)
{
    const size_t len = 1 + UINT32_INDEX_LEN;
    ptrdiff_t offset;
    if (!emitCheck(JSOP_NEWINIT, len, &offset))
        return false;

    jsbytecode* code = this->code(offset);
    code[0] = JSOP_NEWINIT;
    code[1] = jsbytecode(key);
    code[2] = 0;
    code[3] = 0;
    code[4] = 0;
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::iteratorResultShape(unsigned* shape)
{
    // No need to do any guessing for the object kind, since we know exactly how
    // many properties we plan to have.
    gc::AllocKind kind = gc::GetGCObjectKind(2);
    RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx, kind, TenuredObject));
    if (!obj)
        return false;

    Rooted<jsid> value_id(cx, AtomToId(cx->names().value));
    Rooted<jsid> done_id(cx, AtomToId(cx->names().done));
    if (!NativeDefineProperty(cx, obj, value_id, UndefinedHandleValue, nullptr, nullptr,
                              JSPROP_ENUMERATE))
    {
        return false;
    }
    if (!NativeDefineProperty(cx, obj, done_id, UndefinedHandleValue, nullptr, nullptr,
                              JSPROP_ENUMERATE))
    {
        return false;
    }

    ObjectBox* objbox = parser->newObjectBox(obj);
    if (!objbox)
        return false;

    *shape = objectList.add(objbox);

    return true;
}

bool
BytecodeEmitter::emitPrepareIteratorResult()
{
    unsigned shape;
    if (!iteratorResultShape(&shape))
        return false;
    return emitIndex32(JSOP_NEWOBJECT, shape);
}

bool
BytecodeEmitter::emitFinishIteratorResult(bool done)
{
    uint32_t value_id;
    if (!makeAtomIndex(cx->names().value, &value_id))
        return false;
    uint32_t done_id;
    if (!makeAtomIndex(cx->names().done, &done_id))
        return false;

    if (!emitIndex32(JSOP_INITPROP, value_id))
        return false;
    if (!emit1(done ? JSOP_TRUE : JSOP_FALSE))
        return false;
    if (!emitIndex32(JSOP_INITPROP, done_id))
        return false;
    return true;
}

bool
BytecodeEmitter::emitToIteratorResult(bool done)
{
    if (!emitPrepareIteratorResult())    // VALUE OBJ
        return false;
    if (!emit1(JSOP_SWAP))               // OBJ VALUE
        return false;
    if (!emitFinishIteratorResult(done)) // RESULT
        return false;
    return true;
}

bool
BytecodeEmitter::emitGetNameAtLocation(JSAtom* name, const NameLocation& loc)
{
    NameOpEmitter noe(this, name, loc, NameOpEmitter::Kind::Get);
    if (!noe.emitGet()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitGetName(ParseNode* pn)
{
    return emitGetName(pn->name());
}

bool
BytecodeEmitter::emitTDZCheckIfNeeded(JSAtom* name, const NameLocation& loc)
{
    // Dynamic accesses have TDZ checks built into their VM code and should
    // never emit explicit TDZ checks.
    MOZ_ASSERT(loc.hasKnownSlot());
    MOZ_ASSERT(loc.isLexical());

    Maybe<MaybeCheckTDZ> check = innermostTDZCheckCache->needsTDZCheck(this, name);
    if (!check)
        return false;

    // We've already emitted a check in this basic block.
    if (*check == DontCheckTDZ)
        return true;

    if (loc.kind() == NameLocation::Kind::FrameSlot) {
        if (!emitLocalOp(JSOP_CHECKLEXICAL, loc.frameSlot()))
            return false;
    } else {
        if (!emitEnvCoordOp(JSOP_CHECKALIASEDLEXICAL, loc.environmentCoordinate()))
            return false;
    }

    return innermostTDZCheckCache->noteTDZCheck(this, name, DontCheckTDZ);
}

bool
BytecodeEmitter::emitPropLHS(PropertyAccess* prop)
{
    MOZ_ASSERT(!prop->isSuper());

    ParseNode* expr = &prop->expression();

    if (!expr->is<PropertyAccess>() || expr->as<PropertyAccess>().isSuper()) {
        // The non-optimized case.
        return emitTree(expr);
    }

    /*
     * If the object operand is also a dotted property reference, reverse the
     * list linked via pn_left temporarily so we can iterate over it from the
     * bottom up (reversing again as we go), to avoid excessive recursion.
     */
    PropertyAccess* pndot = &expr->as<PropertyAccess>();
    ParseNode* pnup = nullptr;
    ParseNode* pndown;
    for (;;) {
        /* Reverse pndot->pn_left to point up, not down. */
        pndown = &pndot->expression();
        pndot->setExpression(pnup);
        if (!pndown->is<PropertyAccess>() || pndown->as<PropertyAccess>().isSuper())
            break;
        pnup = pndot;
        pndot = &pndown->as<PropertyAccess>();
    }

    /* pndown is a primary expression, not a dotted property reference. */
    if (!emitTree(pndown))
        return false;

    while (true) {
        /* Walk back up the list, emitting annotated name ops. */
        if (!emitAtomOp(pndot->key().atom(), JSOP_GETPROP))
            return false;

        /* Reverse the pn_left link again. */
        pnup = pndot->maybeExpression();
        pndot->setExpression(pndown);
        pndown = pndot;
        if (!pnup) {
            break;
        }
        pndot = &pnup->as<PropertyAccess>();
    }
    return true;
}

bool
BytecodeEmitter::emitPropIncDec(UnaryNode* incDec)
{
    PropertyAccess* prop = &incDec->kid()->as<PropertyAccess>();
    bool isSuper = prop->isSuper();

    ParseNodeKind kind = incDec->getKind();
    PropOpEmitter poe(this,
                      kind == PNK_POSTINCREMENT ? PropOpEmitter::Kind::PostIncrement
                      : kind == PNK_PREINCREMENT ? PropOpEmitter::Kind::PreIncrement
                      : kind == PNK_POSTDECREMENT ? PropOpEmitter::Kind::PostDecrement
                      : PropOpEmitter::Kind::PreDecrement,
                      isSuper
                      ? PropOpEmitter::ObjKind::Super
                      : PropOpEmitter::ObjKind::Other);
    if (!poe.prepareForObj()) {
        return false;
    }
    if (isSuper) {
        UnaryNode* base = &prop->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {              // THIS
            return false;
        }
    } else {
        if (!emitPropLHS(prop))                            // OBJ
            return false;
    }
    if (!poe.emitIncDec(prop->key().atom())) {           // RESULT
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitNameIncDec(UnaryNode* incDec)
{
    MOZ_ASSERT(incDec->kid()->isKind(PNK_NAME));

    ParseNodeKind kind = incDec->getKind();
    NameNode* name = &incDec->kid()->as<NameNode>();
    NameOpEmitter noe(this, name->atom(),
                      kind == PNK_POSTINCREMENT ? NameOpEmitter::Kind::PostIncrement
                      : kind == PNK_PREINCREMENT ? NameOpEmitter::Kind::PreIncrement
                      : kind == PNK_POSTDECREMENT ? NameOpEmitter::Kind::PostDecrement
                      : NameOpEmitter::Kind::PreDecrement);
    if (!noe.emitIncDec()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitElemOpBase(JSOp op)
{
    if (!emit1(op))
        return false;

    return true;
}

bool
BytecodeEmitter::emitElemObjAndKey(PropertyByValue* elem, bool isSuper, ElemOpEmitter& eoe)
{
    if (isSuper) {
        if (!eoe.prepareForObj()) {                       //
            return false;
        }
        UnaryNode* base = &elem->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {             // THIS
            return false;
        }
        if (!eoe.prepareForKey()) {                       // THIS
            return false;
        }
        if (!emitTree(&elem->key())) {                    // THIS KEY
            return false;
        }

        return true;
    }

    if (!eoe.prepareForObj()) {                           //
        return false;
    }
    if (!emitTree(&elem->expression())) {                 // OBJ
        return false;
    }
    if (!eoe.prepareForKey()) {                           // OBJ? OBJ
        return false;
    }
    if (!emitTree(&elem->key())) {                        // OBJ? OBJ KEY
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitElemIncDec(UnaryNode* incDec)
{
    PropertyByValue* elem = &incDec->kid()->as<PropertyByValue>();
    bool isSuper = elem->isSuper();
    ParseNodeKind kind = incDec->getKind();
    ElemOpEmitter eoe(this,
                      kind == PNK_POSTINCREMENT ? ElemOpEmitter::Kind::PostIncrement
                      : kind == PNK_PREINCREMENT ? ElemOpEmitter::Kind::PreIncrement
                      : kind == PNK_POSTDECREMENT ? ElemOpEmitter::Kind::PostDecrement
                      : ElemOpEmitter::Kind::PreDecrement,
                      isSuper
                      ? ElemOpEmitter::ObjKind::Super
                      : ElemOpEmitter::ObjKind::Other);
    if (!emitElemObjAndKey(elem, isSuper, eoe)) {     // [Super]
        //                                                // THIS KEY
        //                                                // [Other]
        //                                                // OBJ KEY
        return false;
    }
    if (!eoe.emitIncDec()) {                              // RESULT
         return false;
    }

    return true;
}

bool
BytecodeEmitter::emitCallIncDec(UnaryNode* incDec)
{
    MOZ_ASSERT(incDec->isKind(PNK_PREINCREMENT) ||
               incDec->isKind(PNK_POSTINCREMENT) ||
               incDec->isKind(PNK_PREDECREMENT) ||
               incDec->isKind(PNK_POSTDECREMENT));

    ParseNode* call = incDec->kid();
    MOZ_ASSERT(call->isKind(PNK_CALL));
    if (!emitTree(call))                                // CALLRESULT
        return false;
    if (!emit1(JSOP_POS))                               // N
        return false;

    // The increment/decrement has no side effects, so proceed to throw for
    // invalid assignment target.
    return emitUint16Operand(JSOP_THROWMSG, JSMSG_BAD_LEFTSIDE_OF_ASS);
}

bool
BytecodeEmitter::emitNumberOp(double dval)
{
    int32_t ival;
    if (NumberIsInt32(dval, &ival)) {
        if (ival == 0)
            return emit1(JSOP_ZERO);
        if (ival == 1)
            return emit1(JSOP_ONE);
        if ((int)(int8_t)ival == ival)
            return emit2(JSOP_INT8, uint8_t(int8_t(ival)));

        uint32_t u = uint32_t(ival);
        if (u < JS_BIT(16)) {
            if (!emitUint16Operand(JSOP_UINT16, u))
                return false;
        } else if (u < JS_BIT(24)) {
            ptrdiff_t off;
            if (!emitN(JSOP_UINT24, 3, &off))
                return false;
            SET_UINT24(code(off), u);
        } else {
            ptrdiff_t off;
            if (!emitN(JSOP_INT32, 4, &off))
                return false;
            SET_INT32(code(off), ival);
        }
        return true;
    }

    if (!constList.append(DoubleValue(dval)))
        return false;

    return emitIndex32(JSOP_DOUBLE, constList.length() - 1);
}

/*
 * Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047.
 * LLVM is deciding to inline this function which uses a lot of stack space
 * into emitTree which is recursive and uses relatively little stack space.
 */
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitSwitch(SwitchStatement* switchStmt)
{
    LexicalScopeNode& lexical = switchStmt->lexicalForCaseList();
    ListNode* cases = &lexical.scopeBody()->as<ListNode>();
    MOZ_ASSERT(cases->isKind(PNK_STATEMENTLIST));

    SwitchEmitter se(this);
    if (!se.emitDiscriminant(Some(switchStmt->pn_pos.begin)))
        return false;
    if (!emitTree(&switchStmt->discriminant()))
        return false;

    // Enter the scope before pushing the switch BreakableControl since all
    // breaks are under this scope.
    if (!lexical.isEmptyScope()) {
        if (!se.emitLexical(lexical.scopeBindings()))
            return false;

        // A switch statement may contain hoisted functions inside its
        // cases. The PNX_FUNCDEFS flag is propagated from the STATEMENTLIST
        // bodies of the cases to the case list.
        if (cases->hasTopLevelFunctionDeclarations()) {
            for (ParseNode* item : cases->contents()) {
                CaseClause* caseClause = &item->as<CaseClause>();
                ListNode* statements = caseClause->statementList();
                if (statements->hasTopLevelFunctionDeclarations()) {
                    if (!emitHoistedFunctionsInList(statements))
                        return false;
                }
            }
        }
    } else {
        MOZ_ASSERT(!cases->hasTopLevelFunctionDeclarations());
    }

    SwitchEmitter::TableGenerator tableGen(this);
    uint32_t caseCount = cases->count() - (switchStmt->hasDefault() ? 1 : 0);
    if (caseCount == 0) {
        tableGen.finish(0);
    } else {
        for (ParseNode* item : cases->contents()) {
            CaseClause* caseClause = &item->as<CaseClause>();
            if (caseClause->isDefault())
                continue;

            ParseNode* caseValue = caseClause->caseExpression();

            if (caseValue->getKind() != PNK_NUMBER) {
                tableGen.setInvalid();
                break;
            }

            int32_t i;
            if (!NumberIsInt32(caseValue->as<NumericLiteral>().value(), &i)) {
                tableGen.setInvalid();
                break;
            }

            if (!tableGen.addNumber(i))
                return false;
        }

        tableGen.finish(caseCount);
    }
    if (!se.validateCaseCount(caseCount))
        return false;

    bool isTableSwitch = tableGen.isValid();
    if (isTableSwitch) {
        if (!se.emitTable(tableGen))
            return false;
    } else {
        if (!se.emitCond())
            return false;

        // Emit code for evaluating cases and jumping to case statements.
        for (ParseNode* item : cases->contents()) {
            CaseClause* caseClause = &item->as<CaseClause>();
            if (caseClause->isDefault())
                continue;

            ParseNode* caseValue = caseClause->caseExpression();

            // If the expression is a literal, suppress line number emission so
            // that debugging works more naturally.
            if (!emitTree(caseValue, ValueUsage::WantValue,
                          caseValue->isLiteral() ? SUPPRESS_LINENOTE : EMIT_LINENOTE))
            {
                return false;
            }
            if (!se.emitCaseJump())
                return false;
        }
    }

    // Emit code for each case's statements.
    for (ParseNode* item : cases->contents()) {
        CaseClause* caseClause = &item->as<CaseClause>();
        if (caseClause->isDefault()) {
            if (!se.emitDefaultBody())
                return false;
        } else {
            if (isTableSwitch) {
                ParseNode* caseValue = caseClause->caseExpression();
                MOZ_ASSERT(caseValue->isKind(PNK_NUMBER));

                NumericLiteral* literal = &caseValue->as<NumericLiteral>();
#ifdef DEBUG
                // Use NumberEqualsInt32 here because switches compare using
                // strict equality, which will equate -0 and +0.  In contrast
                // NumberIsInt32 would return false for -0.
                int32_t v;
                MOZ_ASSERT(mozilla::NumberEqualsInt32(literal->value(), &v));
#endif
                int32_t i = int32_t(literal->value());

                if (!se.emitCaseBody(i, tableGen))
                    return false;
            } else {
                if (!se.emitCaseBody())
                    return false;
            }
        }

        if (!emitTree(caseClause->statementList()))
            return false;
    }

    if (!se.emitEnd())
        return false;

    return true;
}

bool
BytecodeEmitter::isRunOnceLambda()
{
    // The run once lambda flags set by the parser are approximate, and we look
    // at properties of the function itself before deciding to emit a function
    // as a run once lambda.

    if (!(parent && parent->emittingRunOnceLambda) &&
        (emitterMode != LazyFunction || !lazyScript->treatAsRunOnce()))
    {
        return false;
    }

    FunctionBox* funbox = sc->asFunctionBox();
    return !funbox->argumentsHasLocalBinding() &&
           !funbox->isStarGenerator() &&
           !funbox->isLegacyGenerator() &&
           !funbox->isAsync() &&
           !funbox->function()->explicitName();
}

bool
BytecodeEmitter::emitYieldOp(JSOp op)
{
    if (op == JSOP_FINALYIELDRVAL)
        return emit1(JSOP_FINALYIELDRVAL);

    MOZ_ASSERT(op == JSOP_INITIALYIELD || op == JSOP_YIELD || op == JSOP_AWAIT);

    ptrdiff_t off;
    if (!emitN(op, 3, &off))
        return false;

    uint32_t yieldAndAwaitIndex = yieldAndAwaitOffsetList.length();
    if (yieldAndAwaitIndex >= JS_BIT(24)) {
        reportError(nullptr, JSMSG_TOO_MANY_YIELDS);
        return false;
    }

    if (op == JSOP_YIELD)
        yieldAndAwaitOffsetList.numYields++;
    else
        yieldAndAwaitOffsetList.numAwaits++;

    SET_UINT24(code(off), yieldAndAwaitIndex);

    if (!yieldAndAwaitOffsetList.append(offset()))
        return false;

    return emit1(JSOP_DEBUGAFTERYIELD);
}

bool
BytecodeEmitter::emitSetThis(BinaryNode* setThisNode)
{
    // PNK_SETTHIS is used to update |this| after a super() call in a derived
    // class constructor.

    MOZ_ASSERT(setThisNode->isKind(PNK_SETTHIS));
    MOZ_ASSERT(setThisNode->left()->isKind(PNK_NAME));

    RootedAtom name(cx, setThisNode->left()->name());

    // The 'this' binding is not lexical, but due to super() semantics this
    // initialization needs to be treated as a lexical one.
    NameLocation loc = lookupName(name);
    NameLocation lexicalLoc;
    if (loc.kind() == NameLocation::Kind::FrameSlot) {
        lexicalLoc = NameLocation::FrameSlot(BindingKind::Let, loc.frameSlot());
    } else if (loc.kind() == NameLocation::Kind::EnvironmentCoordinate) {
        EnvironmentCoordinate coord = loc.environmentCoordinate();
        uint8_t hops = AssertedCast<uint8_t>(coord.hops());
        lexicalLoc = NameLocation::EnvironmentCoordinate(BindingKind::Let, hops, coord.slot());
    } else {
        MOZ_ASSERT(loc.kind() == NameLocation::Kind::Dynamic);
        lexicalLoc = loc;
    }

    NameOpEmitter noe(this, name, lexicalLoc, NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {                            //
        return false;
    }

    // Emit the new |this| value.
    if (!emitTree(setThisNode->right()))                   // NEWTHIS
        return false;

    // Get the original |this| and throw if we already initialized
    // it. Do *not* use the NameLocation argument, as that's the special
    // lexical location below to deal with super() semantics.
    if (!emitGetName(name)) {                              // NEWTHIS THIS
        return false;
    }
    if (!emit1(JSOP_CHECKTHISREINIT)) {                   // NEWTHIS THIS
        return false;
    }
    if (!emit1(JSOP_POP)) {                               // NEWTHIS
        return false;
    }
    if (!noe.emitAssignment()) {                          // NEWTHIS
        return false;
    }

    if (!emitInitializeInstanceFields()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitScript(ParseNode* body)
{
    setScriptStartOffsetIfUnset(body->pn_pos.begin);

    TDZCheckCache tdzCache(this);
    EmitterScope emitterScope(this);
    if (sc->isGlobalContext()) {
        switchToPrologue();
        if (!emitterScope.enterGlobal(this, sc->asGlobalContext()))
            return false;
        switchToMain();
    } else if (sc->isEvalContext()) {
        switchToPrologue();
        if (!emitterScope.enterEval(this, sc->asEvalContext()))
            return false;
        switchToMain();
    } else {
        MOZ_ASSERT(sc->isModuleContext());
        if (!emitterScope.enterModule(this, sc->asModuleContext()))
            return false;
    }

    setFunctionBodyEndPos(body->pn_pos.end);

    if (sc->isEvalContext() && !sc->strict() &&
        body->is<LexicalScopeNode>() && !body->as<LexicalScopeNode>().isEmptyScope())
    {
        // Sloppy eval scripts may need to emit DEFFUNs in the prologue. If there is
        // an immediately enclosed lexical scope, we need to enter the lexical
        // scope in the prologue for the DEFFUNs to pick up the right
        // environment chain.
        EmitterScope lexicalEmitterScope(this);
        LexicalScopeNode* scope = &body->as<LexicalScopeNode>();

        switchToPrologue();
        if (!lexicalEmitterScope.enterLexical(this, ScopeKind::Lexical, scope->scopeBindings()))
            return false;
        switchToMain();

        if (!emitLexicalScopeBody(scope->scopeBody()))
            return false;

        if (!lexicalEmitterScope.leave(this))
            return false;
    } else {
        if (!emitTree(body))
            return false;
    }

    if (!emit1(JSOP_RETRVAL))
        return false;

    if (!emitterScope.leave(this))
        return false;

    if (!JSScript::fullyInitFromEmitter(cx, script, this))
        return false;

    // URL and source map information must be set before firing
    // Debugger::onNewScript.
    if (!maybeSetDisplayURL() || !maybeSetSourceMap())
        return false;

    tellDebuggerAboutCompiledScript(cx);

    return true;
}

bool
BytecodeEmitter::emitFunctionScript(FunctionNode* funNode)
{
    ListNode* paramsBody = &funNode->body()->as<ListNode>();
    FunctionBox* funbox = sc->asFunctionBox();

    MOZ_ASSERT(fieldInitializers_.valid == (funbox->function()->kind() ==
                                            JSFunction::FunctionKind::ClassConstructor));

    setScriptStartOffsetIfUnset(paramsBody->pn_pos.begin);

    //                [stack]

    FunctionScriptEmitter fse(this, funbox, Some(paramsBody->pn_pos.begin),
                              Some(paramsBody->pn_pos.end));
    if (!fse.prepareForParameters()) {
        //              [stack]
        return false;
    }

    if (!emitFunctionFormalParameters(paramsBody)) {
        //              [stack]
        return false;
    }

    if (!fse.prepareForBody()) {
        //              [stack]
        return false;
    }

    if (!emitTree(paramsBody->last())) {
        //              [stack]
        return false;
    }

    if (!fse.emitEndBody()) {
        //              [stack]
        return false;
    }

    if (!fse.initScript())
        return false;

    script->setFieldInitializers(fieldInitializers_);

    return true;
}

bool
BytecodeEmitter::emitDestructuringLHSRef(ParseNode* target, size_t* emitted)
{
    *emitted = 0;

    if (target->isKind(PNK_SPREAD))
        target = target->as<UnaryNode>().kid();
    else if (target->isKind(PNK_ASSIGN))
        target = target->as<AssignmentNode>().left();

    // No need to recur into PNK_ARRAY and PNK_OBJECT subpatterns here, since
    // emitSetOrInitializeDestructuring does the recursion when setting or
    // initializing value.  Getting reference doesn't recur.
    if (target->isKind(PNK_NAME) || target->isKind(PNK_ARRAY) || target->isKind(PNK_OBJECT))
        return true;

#ifdef DEBUG
    int depth = stackDepth;
#endif

    switch (target->getKind()) {
      case PNK_DOT: {
        PropertyAccess* prop = &target->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        PropOpEmitter poe(this,
                          PropOpEmitter::Kind::SimpleAssignment,
                          isSuper
                          ? PropOpEmitter::ObjKind::Super
                          : PropOpEmitter::ObjKind::Other);
        if (!poe.prepareForObj()) {
            return false;
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {         // THIS SUPERBASE
                return false;
            }
            // SUPERBASE is pushed onto THIS in poe.prepareForRhs below.
            *emitted = 2;
        } else {
            if (!emitTree(&prop->expression()))           // OBJ
                return false;
            *emitted = 1;
        }
        if (!poe.prepareForRhs()) {                       // [Super]
            //                                            // THIS SUPERBASE
            //                                            // [Other]
            //                                            // OBJ
            return false;
        }
        break;
      }

      case PNK_ELEM: {
        PropertyByValue* elem = &target->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter eoe(this,
                          ElemOpEmitter::Kind::SimpleAssignment,
                          isSuper
                          ? ElemOpEmitter::ObjKind::Super
                          : ElemOpEmitter::ObjKind::Other);
        if (!emitElemObjAndKey(elem, isSuper, eoe)) {     // [Super]
            //                                            // THIS KEY
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        if (isSuper) {
            // SUPERBASE is pushed onto KEY in eoe.prepareForRhs below.
            *emitted = 3;
        } else {
            *emitted = 2;
        }
        if (!eoe.prepareForRhs()) {                       // [Super]
            //                                            // THIS KEY SUPERBASE
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        break;
      }

      case PNK_CALL:
        MOZ_ASSERT_UNREACHABLE("Parser::reportIfNotValidSimpleAssignmentTarget "
                               "rejects function calls as assignment "
                               "targets in destructuring assignments");
        break;

      default:
        MOZ_CRASH("emitDestructuringLHSRef: bad lhs kind");
    }

    MOZ_ASSERT(stackDepth == depth + int(*emitted));

    return true;
}

bool
BytecodeEmitter::emitSetOrInitializeDestructuring(ParseNode* target, DestructuringFlavor flav)
{
    // Now emit the lvalue opcode sequence. If the lvalue is a nested
    // destructuring initialiser-form, call ourselves to handle it, then pop
    // the matched value. Otherwise emit an lvalue bytecode sequence followed
    // by an assignment op.
    if (target->isKind(PNK_SPREAD))
        target = target->as<UnaryNode>().kid();
    else if (target->isKind(PNK_ASSIGN))
        target = target->as<AssignmentNode>().left();
    if (target->isKind(PNK_ARRAY) || target->isKind(PNK_OBJECT)) {
        if (!emitDestructuringOps(&target->as<ListNode>(), flav))
            return false;
        // Per its post-condition, emitDestructuringOps has left the
        // to-be-destructured value on top of the stack.
        if (!emit1(JSOP_POP))
            return false;
    } else {
        switch (target->getKind()) {
          case PNK_NAME: {
            RootedAtom name(cx, target->name());
            NameLocation loc;
            NameOpEmitter::Kind kind;
            switch (flav) {
              case DestructuringDeclaration:
                loc = lookupName(name);
                kind = NameOpEmitter::Kind::Initialize;
                break;

              case DestructuringFormalParameterInVarScope: {
                // If there's an parameter expression var scope, the
                // destructuring declaration needs to initialize the name in
                // the function scope. The innermost scope is the var scope,
                // and its enclosing scope is the function scope.
                EmitterScope* funScope = innermostEmitterScope()->enclosingInFrame();
                loc = *locationOfNameBoundInScope(name, funScope);
                kind = NameOpEmitter::Kind::Initialize;
                break;
              }

              case DestructuringAssignment:
                loc = lookupName(name);
                kind = NameOpEmitter::Kind::SimpleAssignment;
                break;
            }

            NameOpEmitter noe(this, name, loc, kind);
            if (!noe.prepareForRhs()) {                    // V ENV?
                return false;
            }
            if (noe.emittedBindOp()) {
                // This is like ordinary assignment, but with one difference.
                //
                // In `a = b`, we first determine a binding for `a` (using
                // JSOP_BINDNAME or JSOP_BINDGNAME), then we evaluate `b`, then
                // a JSOP_SETNAME instruction.
                //
                // In `[a] = [b]`, per spec, `b` is evaluated first, then we
                // determine a binding for `a`. Then we need to do assignment--
                // but the operands are on the stack in the wrong order for
                // JSOP_SETPROP, so we have to add a JSOP_SWAP.
                //
                // In the cases where we are emitting a name op, emit a swap
                // because of this.
                if (!emit1(JSOP_SWAP)) {                   // ENV V
                    return false;
                }
            } else {
                // In cases of emitting a frame slot or environment slot,
                // nothing needs be done.
            }
            if (!noe.emitAssignment()) {                  // V
                return false;
            }
            break;
          }

          case PNK_DOT: {
            // The reference is already pushed by emitDestructuringLHSRef.
            //                                             // [Super]
            //                                             // THIS SUPERBASE VAL
            //                                             // [Other]
            //                                             // OBJ VAL
            PropertyAccess* prop = &target->as<PropertyAccess>();
            bool isSuper = prop->isSuper();
            PropOpEmitter poe(this,
                              PropOpEmitter::Kind::SimpleAssignment,
                              isSuper
                              ? PropOpEmitter::ObjKind::Super
                              : PropOpEmitter::ObjKind::Other);
            if (!poe.skipObjAndRhs()) {
                return false;
            }
            if (!poe.emitAssignment(prop->key().atom())) { // VAL
                return false;
            }
            break;
          }

          case PNK_ELEM: {
            // The reference is already pushed by emitDestructuringLHSRef.
            //                                             // [Super]
            //                                             // THIS KEY SUPERBASE VAL
            //                                             // [Other]
            //                                             // OBJ KEY VAL
            PropertyByValue* elem = &target->as<PropertyByValue>();
            bool isSuper = elem->isSuper();
            ElemOpEmitter eoe(this,
                              ElemOpEmitter::Kind::SimpleAssignment,
                              isSuper
                              ? ElemOpEmitter::ObjKind::Super
                              : ElemOpEmitter::ObjKind::Other);
            if (!eoe.skipObjAndKeyAndRhs()) {
                return false;
            }
            if (!eoe.emitAssignment()) {                   // VAL
                return false;
            }
            break;
          }

          case PNK_CALL:
            MOZ_ASSERT_UNREACHABLE("Parser::reportIfNotValidSimpleAssignmentTarget "
                                   "rejects function calls as assignment "
                                   "targets in destructuring assignments");
            break;

          default:
            MOZ_CRASH("emitSetOrInitializeDestructuring: bad lhs kind");
        }

        // Pop the assigned value.
        if (!emit1(JSOP_POP))                              // !STACK EMPTY!
            return false;
    }

    return true;
}

bool BytecodeEmitter::emitPushNotUndefinedOrNull()
{
    //                [stack] V
    MOZ_ASSERT(stackDepth > 0);

    if (!emit1(JSOP_DUP)) {
        //            [stack] V V
        return false;
    }
    if (!emit1(JSOP_UNDEFINED)) {
        //            [stack] V V UNDEFINED
        return false;
    }
    if (!emit1(JSOP_NE)) {
        //            [stack] V NEQ
        return false;
    }

    JumpList undefinedOrNullJump;
    if (!emitJump(JSOP_AND, &undefinedOrNullJump)) {
        //            [stack] V NEQ
        return false;
    }

    if (!emit1(JSOP_POP)) {
        //            [stack] V
        return false;
    }
    if (!emit1(JSOP_DUP)) {
        //            [stack] V V
        return false;
    }
    if (!emit1(JSOP_NULL)) {
        //            [stack] V V NULL
        return false;
    }
    if (!emit1(JSOP_NE)) {
        //            [stack] V NEQ
        return false;
    }

    if (!emitJumpTargetAndPatch(undefinedOrNullJump)) {
        //            [stack] V NOT-UNDEF-OR-NULL
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitIteratorNext(ParseNode* pn, IteratorKind iterKind /* = IteratorKind::Sync */,
                                  bool allowSelfHosted /* = false */)
{
    MOZ_ASSERT(allowSelfHosted || emitterMode != BytecodeEmitter::SelfHosting,
               ".next() iteration is prohibited in self-hosted code because it "
               "can run user-modifiable iteration code");

    if (!emit1(JSOP_DUP))                                 // ... ITER ITER
        return false;
    if (!emitAtomOp(cx->names().next, JSOP_CALLPROP))     // ... ITER NEXT
        return false;
    if (!emit1(JSOP_SWAP))                                // ... NEXT ITER
        return false;
    if (!emitCall(JSOP_CALL, 0, pn))                      // ... RESULT
        return false;

    if (iterKind == IteratorKind::Async) {
        if (!emitAwaitInInnermostScope())                 // ... RESULT
            return false;
    }

    if (!emitCheckIsObj(CheckIsObjectKind::IteratorNext)) // ... RESULT
        return false;
    return true;
}

bool
BytecodeEmitter::emitIteratorCloseInScope(EmitterScope& currentScope,
                                          IteratorKind iterKind /* = IteratorKind::Sync */,
                                          CompletionKind completionKind /* = CompletionKind::Normal */,
                                          bool allowSelfHosted /* = false */)
{
    MOZ_ASSERT(allowSelfHosted || emitterMode != BytecodeEmitter::SelfHosting,
               ".close() on iterators is prohibited in self-hosted code because it "
               "can run user-modifiable iteration code");

    // Generate inline logic corresponding to IteratorClose (ES 7.4.6).
    //
    // Callers need to ensure that the iterator object is at the top of the
    // stack.

    if (!emit1(JSOP_DUP))                                 // ... ITER ITER
        return false;

    // Step 3.
    //
    // Get the "return" method.
    if (!emitAtomOp(cx->names().return_, JSOP_CALLPROP))  // ... ITER RET
        return false;

    // Step 4.
    //
    // Do nothing if "return" is null or undefined.
    InternalIfEmitter ifReturnMethodIsDefined(this);
    if (!emit1(JSOP_DUP))                                 // ... ITER RET RET
        return false;
    if (!emit1(JSOP_UNDEFINED))                           // ... ITER RET RET UNDEFINED
        return false;
    if (!emit1(JSOP_NE))                                  // ... ITER RET ?NEQL
        return false;
    if (!ifReturnMethodIsDefined.emitThenElse())
        return false;

    if (completionKind == CompletionKind::Throw) {
        // 7.4.6 IteratorClose ( iterator, completion )
        //   ...
        //   3. Let return be ? GetMethod(iterator, "return").
        //   4. If return is undefined, return Completion(completion).
        //   5. Let innerResult be Call(return, iterator, « »).
        //   6. If completion.[[Type]] is throw, return Completion(completion).
        //   7. If innerResult.[[Type]] is throw, return
        //      Completion(innerResult).
        //
        // For CompletionKind::Normal case, JSOP_CALL for step 5 checks if RET
        // is callable, and throws if not.  Since step 6 doesn't match and
        // error handling in step 3 and step 7 can be merged.
        //
        // For CompletionKind::Throw case, an error thrown by JSOP_CALL for
        // step 5 is ignored by try-catch.  So we should check if RET is
        // callable here, outside of try-catch, and the throw immediately if
        // not.
        CheckIsCallableKind kind = CheckIsCallableKind::IteratorReturn;
        if (!emitCheckIsCallable(kind))                   // ... ITER RET
            return false;
    }

    // Steps 5, 8.
    //
    // Call "return" if it is not undefined or null, and check that it returns
    // an Object.
    if (!emit1(JSOP_SWAP))                                // ... RET ITER
        return false;

    Maybe<TryEmitter> tryCatch;

    if (completionKind == CompletionKind::Throw) {
        tryCatch.emplace(this, TryEmitter::TryCatch, TryEmitter::DontUseRetVal,
                         TryEmitter::DontUseControl);

        // Mutate stack to balance stack for try-catch.
        if (!emit1(JSOP_UNDEFINED))                       // ... RET ITER UNDEF
            return false;
        if (!tryCatch->emitTry())                         // ... RET ITER UNDEF
            return false;
        if (!emitDupAt(2))                                // ... RET ITER UNDEF RET
            return false;
        if (!emitDupAt(2))                                // ... RET ITER UNDEF RET ITER
            return false;
    }

    if (!emitCall(JSOP_CALL, 0))                          // ... ... RESULT
        return false;

    if (iterKind == IteratorKind::Async) {
        if (completionKind != CompletionKind::Throw) {
            // Await clobbers rval, so save the current rval.
            if (!emit1(JSOP_GETRVAL))                     // ... ... RESULT RVAL
                return false;
            if (!emit1(JSOP_SWAP))                        // ... ... RVAL RESULT
                return false;
        }
        if (!emitAwaitInScope(currentScope))              // ... ... RVAL? RESULT
            return false;
    }

    if (completionKind == CompletionKind::Throw) {
        if (!emit1(JSOP_SWAP))                            // ... RET ITER RESULT UNDEF
            return false;
        if (!emit1(JSOP_POP))                             // ... RET ITER RESULT
            return false;

        if (!tryCatch->emitCatch())                       // ... RET ITER RESULT
            return false;

        // Just ignore the exception thrown by call and await.
        if (!emit1(JSOP_EXCEPTION))                       // ... RET ITER RESULT EXC
            return false;
        if (!emit1(JSOP_POP))                             // ... RET ITER RESULT
            return false;

        if (!tryCatch->emitEnd())                         // ... RET ITER RESULT
            return false;

        // Restore stack.
        if (!emit2(JSOP_UNPICK, 2))                       // ... RESULT RET ITER
            return false;
        if (!emit1(JSOP_POP))                             // ... RESULT RET
            return false;
        if (!emit1(JSOP_POP))                             // ... RESULT
            return false;
    } else {
        if (!emitCheckIsObj(CheckIsObjectKind::IteratorReturn)) // ... RVAL? RESULT
            return false;

        if (iterKind == IteratorKind::Async) {
            if (!emit1(JSOP_SWAP))                        // ... RESULT RVAL
                return false;
            if (!emit1(JSOP_SETRVAL))                     // ... RESULT
                return false;
        }
    }

    if (!ifReturnMethodIsDefined.emitElse())
        return false;
    if (!emit1(JSOP_POP))                                 // ... ITER
        return false;
    if (!ifReturnMethodIsDefined.emitEnd())
        return false;

    return emit1(JSOP_POP);                               // ...
}

template <typename InnerEmitter>
bool
BytecodeEmitter::wrapWithDestructuringIteratorCloseTryNote(int32_t iterDepth, InnerEmitter emitter)
{
    MOZ_ASSERT(this->stackDepth >= iterDepth);

    // Pad a nop at the beginning of the bytecode covered by the trynote so
    // that when unwinding environments, we may unwind to the scope
    // corresponding to the pc *before* the start, in case the first bytecode
    // emitted by |emitter| is the start of an inner scope. See comment above
    // UnwindEnvironmentToTryPc.
    if (!emit1(JSOP_TRY_DESTRUCTURING_ITERCLOSE))
        return false;

    ptrdiff_t start = offset();
    if (!emitter(this))
        return false;
    ptrdiff_t end = offset();
    if (start != end)
        return tryNoteList.append(JSTRY_DESTRUCTURING_ITERCLOSE, iterDepth, start, end);
    return true;
}

bool
BytecodeEmitter::emitDefault(ParseNode* defaultExpr, ParseNode* pattern)
{
    //                [stack] VALUE

    DefaultEmitter de(this);
    if (!de.prepareForDefault()) {
        //              [stack]
        return false;
    }
    if (!emitInitializer(defaultExpr, pattern)) {
        //              [stack] DEFAULTVALUE
        return false;
    }
    if (!de.emitEnd()) {
        //              [stack] VALUE/DEFAULTVALUE
        return false;
    }
    return true;
}

bool
BytecodeEmitter::setOrEmitSetFunName(ParseNode* maybeFun, HandleAtom name)
{
    if (maybeFun->is<FunctionNode>()) {
        // Function doesn't have 'name' property at this point.
        // Set function's name at compile time.
        return setFunName(maybeFun->as<FunctionNode>().funbox()->function(), name);
    }

    MOZ_ASSERT(maybeFun->isKind(PNK_CLASS));

    return emitSetClassConstructorName(name);
}

bool
BytecodeEmitter::setFunName(JSFunction* fun, JSAtom* name)
{
    // Single node can be emitted multiple times if it appears in
    // array destructuring default.  If function already has a name,
    // just return.
    if (fun->hasCompileTimeName()) {
#ifdef DEBUG
        RootedAtom funName(cx, name);
        if (!funName)
            return false;
        MOZ_ASSERT(funName == fun->compileTimeName());
#endif
        return true;
    }

    RootedAtom funName(cx, name);
    if (!funName)
        return false;
    fun->setCompileTimeName(funName);
    return true;
}

bool
BytecodeEmitter::emitSetClassConstructorName(JSAtom* name)
{
    uint32_t nameIndex;
    if (!makeAtomIndex(name, &nameIndex))
        return false;
    if (!emitIndexOp(JSOP_STRING, nameIndex))   // FUN NAME
        return false;
    uint8_t kind = uint8_t(FunctionPrefixKind::None);
    if (!emit2(JSOP_SETFUNNAME, kind))          // FUN
        return false;
    return true;
}

bool
BytecodeEmitter::emitSetFunctionNameFromStack(uint8_t offset)
{
    //                       [stack] KEY ... FUN
    if (!emitDupAt(offset))
        return false;
    //                       [stack] KEY ... FUN KEY
    uint8_t kind = uint8_t(FunctionPrefixKind::None);
    if (!emit2(JSOP_SETFUNNAME, kind)) {
        //                   [stack] KEY ... FUN
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitInitializer(ParseNode* initializer, ParseNode* pattern)
{
    if (!emitTree(initializer))
        return false;

    if (!pattern->isInParens() && pattern->isKind(PNK_NAME) &&
        initializer->isDirectRHSAnonFunction())
    {
        RootedAtom name(cx, pattern->name());
        if (!setOrEmitSetFunName(initializer, name))
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDestructuringOpsArray(ListNode* pattern, DestructuringFlavor flav)
{
    MOZ_ASSERT(pattern->isKind(PNK_ARRAY));
    MOZ_ASSERT(this->stackDepth != 0);

    // Here's pseudo code for |let [a, b, , c=y, ...d] = x;|
    //
    // Lines that are annotated "covered by trynote" mean that upon throwing
    // an exception, IteratorClose is called on iter only if done is false.
    //
    //   let x, y;
    //   let a, b, c, d;
    //   let iter, lref, result, done, value; // stack values
    //
    //   iter = x[Symbol.iterator]();
    //
    //   // ==== emitted by loop for a ====
    //   lref = GetReference(a);              // covered by trynote
    //
    //   result = iter.next();
    //   done = result.done;
    //
    //   if (done)
    //     value = undefined;
    //   else
    //     value = result.value;
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // ==== emitted by loop for b ====
    //   lref = GetReference(b);              // covered by trynote
    //
    //   if (done) {
    //     value = undefined;
    //   } else {
    //     result = iter.next();
    //     done = result.done;
    //     if (done)
    //       value = undefined;
    //     else
    //       value = result.value;
    //   }
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // ==== emitted by loop for elision ====
    //   if (done) {
    //     value = undefined;
    //   } else {
    //     result = iter.next();
    //     done = result.done;
    //     if (done)
    //       value = undefined;
    //     else
    //       value = result.value;
    //   }
    //
    //   // ==== emitted by loop for c ====
    //   lref = GetReference(c);              // covered by trynote
    //
    //   if (done) {
    //     value = undefined;
    //   } else {
    //     result = iter.next();
    //     done = result.done;
    //     if (done)
    //       value = undefined;
    //     else
    //       value = result.value;
    //   }
    //
    //   if (value === undefined)
    //     value = y;                         // covered by trynote
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // ==== emitted by loop for d ====
    //   lref = GetReference(d);              // covered by trynote
    //
    //   if (done)
    //     value = [];
    //   else
    //     value = [...iter];
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // === emitted after loop ===
    //   if (!done)
    //      IteratorClose(iter);

    // Use an iterator to destructure the RHS, instead of index lookup. We
    // must leave the *original* value on the stack.
    if (!emit1(JSOP_DUP))                                         // ... OBJ OBJ
        return false;
    if (!emitIterator())                                          // ... OBJ ITER
        return false;

    // For an empty pattern [], call IteratorClose unconditionally. Nothing
    // else needs to be done.
    if (!pattern->head())
        return emitIteratorCloseInInnermostScope();               // ... OBJ

    // Push an initial FALSE value for DONE.
    if (!emit1(JSOP_FALSE))                                       // ... OBJ ITER FALSE
        return false;

    // JSTRY_DESTRUCTURING_ITERCLOSE expects the iterator and the done value
    // to be the second to top and the top of the stack, respectively.
    // IteratorClose is called upon exception only if done is false.
    int32_t tryNoteDepth = stackDepth;

    for (ParseNode* member : pattern->contents()) {
        bool isFirst = member == pattern->head();
        DebugOnly<bool> hasNext = !!member->pn_next;

        size_t emitted = 0;

        // Spec requires LHS reference to be evaluated first.
        ParseNode* lhsPattern = member;
        if (lhsPattern->isKind(PNK_ASSIGN))
            lhsPattern = lhsPattern->as<AssignmentNode>().left();

        bool isElision = lhsPattern->isKind(PNK_ELISION);
        if (!isElision) {
            auto emitLHSRef = [lhsPattern, &emitted](BytecodeEmitter* bce) {
                return bce->emitDestructuringLHSRef(lhsPattern, &emitted); // ... OBJ ITER DONE *LREF
            };
            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitLHSRef))
                return false;
        }

        // Pick the DONE value to the top of the stack.
        if (emitted) {
            if (!emit2(JSOP_PICK, emitted))                       // ... OBJ ITER *LREF DONE
                return false;
        }

        if (isFirst) {
            // If this element is the first, DONE is always FALSE, so pop it.
            //
            // Non-first elements should emit if-else depending on the
            // member pattern, below.
            if (!emit1(JSOP_POP))                                 // ... OBJ ITER *LREF
                return false;
        }

        if (member->isKind(PNK_SPREAD)) {
            InternalIfEmitter ifThenElse(this);
            if (!isFirst) {
                // If spread is not the first element of the pattern,
                // iterator can already be completed.
                                                                  // ... OBJ ITER *LREF DONE
                if (!ifThenElse.emitThenElse())                   // ... OBJ ITER *LREF
                    return false;

                if (!emitUint32Operand(JSOP_NEWARRAY, 0))         // ... OBJ ITER *LREF ARRAY
                    return false;
                if (!ifThenElse.emitElse())                       // ... OBJ ITER *LREF
                    return false;
            }

            // If iterator is not completed, create a new array with the rest
            // of the iterator.
            if (!emitDupAt(emitted))                              // ... OBJ ITER *LREF ITER
                return false;
            if (!emitUint32Operand(JSOP_NEWARRAY, 0))             // ... OBJ ITER *LREF ITER ARRAY
                return false;
            if (!emitNumberOp(0))                                 // ... OBJ ITER *LREF ITER ARRAY INDEX
                return false;
            if (!emitSpread())                                    // ... OBJ ITER *LREF ARRAY INDEX
                return false;
            if (!emit1(JSOP_POP))                                 // ... OBJ ITER *LREF ARRAY
                return false;

            if (!isFirst) {
                if (!ifThenElse.emitEnd())
                    return false;
                MOZ_ASSERT(ifThenElse.pushed() == 1);
            }

            // At this point the iterator is done. Unpick a TRUE value for DONE above ITER.
            if (!emit1(JSOP_TRUE))                                // ... OBJ ITER *LREF ARRAY TRUE
                return false;
            if (!emit2(JSOP_UNPICK, emitted + 1))                 // ... OBJ ITER TRUE *LREF ARRAY
                return false;

            auto emitAssignment = [member, flav](BytecodeEmitter* bce) {
                return bce->emitSetOrInitializeDestructuring(member, flav); // ... OBJ ITER TRUE
            };
            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitAssignment))
                return false;

            MOZ_ASSERT(!hasNext);
            break;
        }

        ParseNode* pndefault = nullptr;
        if (member->isKind(PNK_ASSIGN))
            pndefault = member->as<AssignmentNode>().right();

        MOZ_ASSERT(!member->isKind(PNK_SPREAD));

        InternalIfEmitter ifAlreadyDone(this);
        if (!isFirst) {
                                                                  // ... OBJ ITER *LREF DONE
            if (!ifAlreadyDone.emitThenElse())                    // ... OBJ ITER *LREF
                return false;

            if (!emit1(JSOP_UNDEFINED))                           // ... OBJ ITER *LREF UNDEF
                return false;
            if (!emit1(JSOP_NOP_DESTRUCTURING))                   // ... OBJ ITER *LREF UNDEF
                return false;

            // The iterator is done. Unpick a TRUE value for DONE above ITER.
            if (!emit1(JSOP_TRUE))                                // ... OBJ ITER *LREF UNDEF TRUE
                return false;
            if (!emit2(JSOP_UNPICK, emitted + 1))                 // ... OBJ ITER TRUE *LREF UNDEF
                return false;

            if (!ifAlreadyDone.emitElse())                        // ... OBJ ITER *LREF
                return false;
        }

        if (emitted) {
            if (!emitDupAt(emitted))                              // ... OBJ ITER *LREF ITER
                return false;
        } else {
            if (!emit1(JSOP_DUP))                                 // ... OBJ ITER *LREF ITER
                return false;
        }
        if (!emitIteratorNext(pattern))                           // ... OBJ ITER *LREF RESULT
            return false;
        if (!emit1(JSOP_DUP))                                     // ... OBJ ITER *LREF RESULT RESULT
            return false;
        if (!emitAtomOp(cx->names().done, JSOP_GETPROP))          // ... OBJ ITER *LREF RESULT DONE
            return false;

        if (!emit1(JSOP_DUP))                                     // ... OBJ ITER *LREF RESULT DONE DONE
            return false;
        if (!emit2(JSOP_UNPICK, emitted + 2))                     // ... OBJ ITER DONE *LREF RESULT DONE
            return false;

        InternalIfEmitter ifDone(this);
        if (!ifDone.emitThenElse())                               // ... OBJ ITER DONE *LREF RESULT
            return false;

        if (!emit1(JSOP_POP))                                     // ... OBJ ITER DONE *LREF
            return false;
        if (!emit1(JSOP_UNDEFINED))                               // ... OBJ ITER DONE *LREF UNDEF
            return false;
        if (!emit1(JSOP_NOP_DESTRUCTURING))                       // ... OBJ ITER DONE *LREF UNDEF
            return false;

        if (!ifDone.emitElse())                                   // ... OBJ ITER DONE *LREF RESULT
            return false;

        if (!emitAtomOp(cx->names().value, JSOP_GETPROP))         // ... OBJ ITER DONE *LREF VALUE
            return false;

        if (!ifDone.emitEnd())
            return false;
        MOZ_ASSERT(ifDone.pushed() == 0);

        if (!isFirst) {
            if (!ifAlreadyDone.emitEnd())
                return false;
            MOZ_ASSERT(ifAlreadyDone.pushed() == 2);
        }

        if (pndefault) {
            auto emitDefault = [pndefault, lhsPattern](BytecodeEmitter* bce) {
                return bce->emitDefault(pndefault, lhsPattern);    // ... OBJ ITER DONE *LREF VALUE
            };

            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitDefault))
                return false;
        }

        if (!isElision) {
            auto emitAssignment = [lhsPattern, flav](BytecodeEmitter* bce) {
                return bce->emitSetOrInitializeDestructuring(lhsPattern, flav); // ... OBJ ITER DONE
            };

            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitAssignment))
                return false;
        } else {
            if (!emit1(JSOP_POP))                                 // ... OBJ ITER DONE
                return false;
        }
    }

    // The last DONE value is on top of the stack. If not DONE, call
    // IteratorClose.
                                                                  // ... OBJ ITER DONE
    InternalIfEmitter ifDone(this);
    if (!ifDone.emitThenElse())                                   // ... OBJ ITER
        return false;
    if (!emit1(JSOP_POP))                                         // ... OBJ
        return false;
    if (!ifDone.emitElse())                                       // ... OBJ ITER
        return false;
    if (!emitIteratorCloseInInnermostScope())                     // ... OBJ
        return false;
    if (!ifDone.emitEnd())
        return false;

    return true;
}

bool
BytecodeEmitter::emitComputedPropertyName(UnaryNode* computedPropName)
{
    MOZ_ASSERT(computedPropName->isKind(PNK_COMPUTED_NAME));
    return emitTree(computedPropName->kid()) && emit1(JSOP_TOID);
}

bool
BytecodeEmitter::emitDestructuringOpsObject(ListNode* pattern, DestructuringFlavor flav)
{
    MOZ_ASSERT(pattern->isKind(PNK_OBJECT));

    MOZ_ASSERT(this->stackDepth > 0);                             // ... RHS

    if (!emitRequireObjectCoercible())                            // ... RHS
        return false;

    bool needsRestPropertyExcludedSet = pattern->count() > 1 &&
                                        pattern->last()->isKind(PNK_SPREAD);
    if (needsRestPropertyExcludedSet) {
        if (!emitDestructuringObjRestExclusionSet(pattern))       // ... RHS SET
            return false;

        if (!emit1(JSOP_SWAP))                                    // ... SET RHS
            return false;
    }

    for (ParseNode* member : pattern->contents()) {
        ParseNode* subpattern;
        if (member->isKind(PNK_MUTATEPROTO) || member->isKind(PNK_SPREAD))
            subpattern = member->as<UnaryNode>().kid();
        else {
            MOZ_ASSERT(member->isKind(PNK_COLON) ||
                       member->isKind(PNK_SHORTHAND));
            subpattern = member->as<BinaryNode>().right();
        }

        ParseNode* lhs = subpattern;
        MOZ_ASSERT_IF(member->isKind(PNK_SPREAD), !lhs->isKind(PNK_ASSIGN));
        if (lhs->isKind(PNK_ASSIGN))
            lhs = lhs->as<AssignmentNode>().left();

        size_t emitted;
        if (!emitDestructuringLHSRef(lhs, &emitted))              // ... *SET RHS *LREF
            return false;

        // Duplicate the value being destructured to use as a reference base.
        if (emitted) {
            if (!emitDupAt(emitted))                              // ... *SET RHS *LREF RHS
                return false;
        } else {
            if (!emit1(JSOP_DUP))                                 // ... *SET RHS RHS
                return false;
        }

        if (member->isKind(PNK_SPREAD)) {
            if (!updateSourceCoordNotes(member->pn_pos.begin))
                return false;

            if (!emitNewInit(JSProto_Object))                     // ... *SET RHS *LREF RHS TARGET
                return false;
            if (!emit1(JSOP_DUP))                                 // ... *SET RHS *LREF RHS TARGET TARGET
                return false;
            if (!emit2(JSOP_PICK, 2))                             // ... *SET RHS *LREF TARGET TARGET RHS
                return false;

            if (needsRestPropertyExcludedSet) {
                if (!emit2(JSOP_PICK, emitted + 4))               // ... RHS *LREF TARGET TARGET RHS SET
                    return false;
            }

            CopyOption option = needsRestPropertyExcludedSet
                                ? CopyOption::Filtered
                                : CopyOption::Unfiltered;
            if (!emitCopyDataProperties(option))                  // ... RHS *LREF TARGET
                return false;

            // Destructure TARGET per this member's lhs.
            if (!emitSetOrInitializeDestructuring(lhs, flav))     // ... RHS
                return false;

            MOZ_ASSERT(member == pattern->last(), "Rest property is always last");
            break;
        }

        // Now push the property name currently being matched, which is the
        // current property name "label" on the left of a colon in the object
        // initialiser.
        bool needsGetElem = true;

        if (member->isKind(PNK_MUTATEPROTO)) {
            if (!emitAtomOp(cx->names().proto, JSOP_GETPROP))     // ... *SET RHS *LREF PROP
                return false;
            needsGetElem = false;
        } else {
            MOZ_ASSERT(member->isKind(PNK_COLON) || member->isKind(PNK_SHORTHAND));

            ParseNode* key = member->as<BinaryNode>().left();
            if (key->isKind(PNK_NUMBER)) {
                if (!emitNumberOp(key->as<NumericLiteral>().value()))
                    return false;                                 // ... *SET RHS *LREF RHS KEY
            } else if (key->isKind(PNK_OBJECT_PROPERTY_NAME) || key->isKind(PNK_STRING)) {
                if (!emitAtomOp(key->as<NameNode>().atom(), JSOP_GETPROP))
                    return false;                                 // ... *SET RHS *LREF PROP
                needsGetElem = false;
            } else {
                if (!emitComputedPropertyName(&key->as<UnaryNode>()))               // ... *SET RHS *LREF RHS KEY
                    return false;

                // Add the computed property key to the exclusion set.
                if (needsRestPropertyExcludedSet) {
                    if (!emitDupAt(emitted + 3))                  // ... SET RHS *LREF RHS KEY SET
                        return false;
                    if (!emitDupAt(1))                            // ... SET RHS *LREF RHS KEY SET KEY
                        return false;
                    if (!emit1(JSOP_UNDEFINED))                   // ... SET RHS *LREF RHS KEY SET KEY UNDEFINED
                        return false;
                    if (!emit1(JSOP_INITELEM))                    // ... SET RHS *LREF RHS KEY SET
                        return false;
                    if (!emit1(JSOP_POP))                         // ... SET RHS *LREF RHS KEY
                        return false;
                }
            }
        }

        // Get the property value if not done already.
        if (needsGetElem && !emitElemOpBase(JSOP_GETELEM))        // ... *SET RHS *LREF PROP
            return false;

        if (subpattern->isKind(PNK_ASSIGN)) {
            if (!emitDefault(subpattern->as<AssignmentNode>().right(), lhs))
                return false;                                     // ... *SET RHS *LREF VALUE
        }

        // Destructure PROP per this member's lhs.
        if (!emitSetOrInitializeDestructuring(subpattern, flav))  // ... *SET RHS
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDestructuringObjRestExclusionSet(ListNode* pattern)
{
    MOZ_ASSERT(pattern->isKind(PNK_OBJECT));
    MOZ_ASSERT(pattern->last()->isKind(PNK_SPREAD));

    ptrdiff_t offset = this->offset();
    if (!emitNewInit(JSProto_Object))
        return false;

    // Try to construct the shape of the object as we go, so we can emit a
    // JSOP_NEWOBJECT with the final shape instead.
    // In the case of computed property names and indices, we cannot fix the
    // shape at bytecode compile time. When the shape cannot be determined,
    // |obj| is nulled out.

    // No need to do any guessing for the object kind, since we know the upper
    // bound of how many properties we plan to have.
    gc::AllocKind kind = gc::GetGCObjectKind(pattern->count() - 1);
    RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx, kind, TenuredObject));
    if (!obj)
        return false;

    RootedAtom pnatom(cx);
    for (ParseNode* member : pattern->contents()) {
        if (member->isKind(PNK_SPREAD))
            break;

        bool isIndex = false;
        if (member->isKind(PNK_MUTATEPROTO)) {
            pnatom.set(cx->names().proto);
        } else {
            ParseNode* key = member->as<BinaryNode>().left();
            if (key->isKind(PNK_NUMBER)) {
                if (!emitNumberOp(key->as<NumericLiteral>().value()))
                    return false;
                isIndex = true;
            } else if (key->isKind(PNK_OBJECT_PROPERTY_NAME) || key->isKind(PNK_STRING)) {
                pnatom.set(key->as<NameNode>().atom());
            } else {
                // Otherwise this is a computed property name which needs to
                // be added dynamically.
                obj.set(nullptr);
                continue;
            }
        }

        // Initialize elements with |undefined|.
        if (!emit1(JSOP_UNDEFINED))
            return false;

        if (isIndex) {
            obj.set(nullptr);
            if (!emit1(JSOP_INITELEM))
                return false;
        } else {
            uint32_t index;
            if (!makeAtomIndex(pnatom, &index))
                return false;

            if (obj) {
                MOZ_ASSERT(!obj->inDictionaryMode());
                Rooted<jsid> id(cx, AtomToId(pnatom));
                if (!NativeDefineProperty(cx, obj, id, UndefinedHandleValue, nullptr, nullptr,
                                          JSPROP_ENUMERATE))
                {
                    return false;
                }
                if (obj->inDictionaryMode())
                    obj.set(nullptr);
            }

            if (!emitIndex32(JSOP_INITPROP, index))
                return false;
        }
    }

    if (obj) {
        // The object survived and has a predictable shape: update the
        // original bytecode.
        if (!replaceNewInitWithNewObject(obj, offset))
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDestructuringOps(ListNode* pattern, DestructuringFlavor flav)
{
    if (pattern->isKind(PNK_ARRAY))
        return emitDestructuringOpsArray(pattern, flav);
    return emitDestructuringOpsObject(pattern, flav);
}

bool
BytecodeEmitter::emitTemplateString(ListNode* templateString)
{
    bool pushedString = false;

    for (ParseNode* item : templateString->contents()) {
        bool isString = (item->getKind() == PNK_STRING || item->getKind() == PNK_TEMPLATE_STRING);

        // Skip empty strings. These are very common: a template string like
        // `${a}${b}` has three empty strings and without this optimization
        // we'd emit four JSOP_ADD operations instead of just one.
        if (isString && item->as<NameNode>().atom()->empty())
            continue;

        if (!isString) {
            // We update source notes before emitting the expression
            if (!updateSourceCoordNotes(item->pn_pos.begin))
                return false;
        }

        if (!emitTree(item))
            return false;

        if (!isString) {
            // We need to convert the expression to a string
            if (!emit1(JSOP_TOSTRING))
                return false;
        }

        if (pushedString) {
            // We've pushed two strings onto the stack. Add them together, leaving just one.
            if (!emit1(JSOP_ADD))
                return false;
        } else {
            pushedString = true;
        }
    }

    if (!pushedString) {
        // All strings were empty, this can happen for something like `${""}`.
        // Just push an empty string.
        if (!emitAtomOp(cx->names().empty, JSOP_STRING))
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDeclarationList(ListNode* declList)
{
    for (ParseNode* decl : declList->contents()) {
        if (!updateSourceCoordNotes(decl->pn_pos.begin))
            return false;

        if (decl->isKind(PNK_ASSIGN)) {
            MOZ_ASSERT(decl->isOp(JSOP_NOP));

            AssignmentNode* assignNode = &decl->as<AssignmentNode>();
            ListNode* pattern = &assignNode->left()->as<ListNode>();
            MOZ_ASSERT(pattern->isKind(PNK_ARRAY) || pattern->isKind(PNK_OBJECT));

            if (!emitTree(assignNode->right()))
                return false;

            if (!emitDestructuringOps(pattern, DestructuringDeclaration))
                return false;

            if (!emit1(JSOP_POP))
                return false;
        } else {
            if (!emitSingleDeclaration(declList, decl, decl->as<NameNode>().initializer()))
                return false;
        }
    }
    return true;
}

bool
BytecodeEmitter::emitSingleDeclaration(ParseNode* declList, ParseNode* decl,
                                       ParseNode* initializer)
{
    MOZ_ASSERT(decl->isKind(PNK_NAME));

    // Nothing to do for initializer-less 'var' declarations, as there's no TDZ.
    if (!initializer && declList->isKind(PNK_VAR))
        return true;

    NameOpEmitter noe(this, decl->name(), NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {                            // ENV?
        return false;
    }
    if (!initializer) {
        // Lexical declarations are initialized to undefined without an
        // initializer.
        MOZ_ASSERT(declList->isKind(PNK_LET),
                   "var declarations without initializers handled above, "
                   "and const declarations must have initializers");
        if (!emit1(JSOP_UNDEFINED)) {                      // ENV? UNDEF
            return false;
        }
    } else {
        MOZ_ASSERT(initializer);
        if (!emitInitializer(initializer, decl)) {         // ENV? V
            return false;
        }
    }
    if (!noe.emitAssignment()) {                           // V
         return false;
    }
    if (!emit1(JSOP_POP)) {                                //
        return false;
    }

    return true;
}

static bool
EmitAssignmentRhs(BytecodeEmitter* bce, ParseNode* rhs, uint8_t offset)
{
    // If there is a RHS tree, emit the tree.
    if (rhs)
        return bce->emitTree(rhs);

    // Otherwise the RHS value to assign is already on the stack, i.e., the
    // next enumeration value in a for-in or for-of loop. Depending on how
    // many other values have been pushed on the stack, we need to get the
    // already-pushed RHS value.
    if (offset != 1 && !bce->emit2(JSOP_PICK, offset - 1))
        return false;

    return true;
}

bool
BytecodeEmitter::emitAssignmentOrInit(ParseNodeKind kind, JSOp compoundOp,
                                      ParseNode* lhs, ParseNode* rhs)
{
    bool isCompound = compoundOp != JSOP_NOP;
    bool isInit = kind == PNK_INITPROP;

    MOZ_ASSERT_IF(isInit, lhs->isKind(PNK_DOT) ||
                          lhs->isKind(PNK_ELEM));

    Maybe<NameOpEmitter> noe;
    Maybe<PropOpEmitter> poe;
    Maybe<ElemOpEmitter> eoe;

    uint8_t offset = 1;
    // Anonymous functions get their inferred name in simple assignments:
    //    x = function() {};                // x.name === "x"
    // In this case, rhs->isDirectRHSAnonFunction() from parsing the statement.
    // To suppress this, put the variable in parentheses:
    //    (x) = function() {};              // x.name === undefined
    // In normal property assignments (`obj.x = function(){}`), the anonymous
    // function does not have a computed name and rhs->isDirectRHSAnonFunction()==false.
    // However, in field initializers (`class C { x = function(){} }`), field
    // initialization is implemented via a property or elem assignment *and*
    // rhs->isDirectRHSAnonFunction() is set. In this case (detected by `isInit`),
    // we'll assign the name of the function using the same plumbing as binding assignments.
    // For PNK_NAME and PNK_DOT, the name is compile-time constant, and is stored in `anonFunctionName`.
    // For PNK_ELEM, we grab it from the stack before emitting the actual assignment.
    RootedAtom anonFunctionName(cx);
    bool inferFunctionName = !isCompound &&
                             rhs && rhs->isDirectRHSAnonFunction() && !lhs->isInParens() &&
                             (lhs->isKind(PNK_NAME) || isInit);

    switch (lhs->getKind()) {
      case PNK_NAME: {
        noe.emplace(this,
                    lhs->name(),
                    isCompound
                    ? NameOpEmitter::Kind::CompoundAssignment
                    : NameOpEmitter::Kind::SimpleAssignment);
        if (inferFunctionName) {
            anonFunctionName = lhs->name();
        }
        break;
      }
      case PNK_DOT: {
        PropertyAccess* prop = &lhs->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        poe.emplace(this,
                    isCompound
                    ? PropOpEmitter::Kind::CompoundAssignment
                    : isInit ? PropOpEmitter::Kind::PropInit
                             : PropOpEmitter::Kind::SimpleAssignment,
                    isSuper
                    ? PropOpEmitter::ObjKind::Super
                    : PropOpEmitter::ObjKind::Other);
        if (!poe->prepareForObj()) {
            return false;
        }
        if (inferFunctionName) {
            anonFunctionName = &prop->name();
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {         // THIS SUPERBASE
                return false;
            }
            // SUPERBASE is pushed onto THIS later in poe->emitGet below.
            offset += 2;
        } else {
            if (!emitTree(&prop->expression()))                    // OBJ
                return false;
            offset += 1;
        }
        break;
      }
      case PNK_ELEM: {
        PropertyByValue* elem = &lhs->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        eoe.emplace(this,
                    isCompound
                    ? ElemOpEmitter::Kind::CompoundAssignment
                    : isInit ? ElemOpEmitter::Kind::PropInit
                             : ElemOpEmitter::Kind::SimpleAssignment,
                    isSuper
                    ? ElemOpEmitter::ObjKind::Super
                    : ElemOpEmitter::ObjKind::Other);
        if (!emitElemObjAndKey(elem, isSuper, *eoe)) {    // [Super]
            //                                            // THIS KEY
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        if (isSuper) {
            // SUPERBASE is pushed onto KEY in eoe->emitGet below.
            offset += 3;
        } else {
            offset += 2;
        }
        break;
      }
      case PNK_ARRAY:
      case PNK_OBJECT:
        break;
      case PNK_CALL:
        if (!emitTree(lhs))
            return false;

        // Assignment to function calls is forbidden, but we have to make the
        // call first.  Now we can throw.
        if (!emitUint16Operand(JSOP_THROWMSG, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return false;

        // Rebalance the stack to placate stack-depth assertions.
        if (!emit1(JSOP_POP))
            return false;
        break;
      default:
        MOZ_ASSERT(0);
    }

    if (isCompound) {
        MOZ_ASSERT(rhs);
        switch (lhs->getKind()) {
          case PNK_DOT: {
            PropertyAccess* prop = &lhs->as<PropertyAccess>();
            if (!poe->emitGet(prop->key().atom())) {       // [Super]
                //                                         // THIS SUPERBASE PROP
                //                                         // [Other]
                //                                         // OBJ PROP
                return false;
            }
            break;
          }
          case PNK_ELEM: {
            if (!eoe->emitGet()) {                        // KEY THIS OBJ ELEM
                return false;
            }
            break;
          }
          case PNK_CALL:
            // We just emitted a JSOP_THROWMSG and popped the call's return
            // value.  Push a random value to make sure the stack depth is
            // correct.
            if (!emit1(JSOP_NULL))
                return false;
            break;
          default:;
        }
    }

    switch (lhs->getKind()) {
      case PNK_NAME:
        if (!noe->prepareForRhs()) {                      // ENV? VAL?
            return false;
        }
        // If we emitted a BIND[G]NAME, then the scope is on
        // the top of the stack and we need to pick the right RHS value.
        if (noe->emittedBindOp())
            offset += 1;
        break;
      case PNK_DOT:
        if (!poe->prepareForRhs()) {                      // [Simple,Super]
            //                                            // THIS SUPERBASE
            //                                            // [Simple,Other]
            //                                            // OBJ
            //                                            // [Compound,Super]
            //                                            // THIS SUPERBASE PROP
            //                                            // [Compound,Other]
            //                                            // OBJ PROP
            return false;
        }
        break;
      case PNK_ELEM:
        if (!eoe->prepareForRhs()) {                      // [Simple,Super]
            //                                            // THIS KEY SUPERBASE
            //                                            // [Simple,Other]
            //                                            // OBJ KEY
            //                                            // [Compound,Super]
            //                                            // THIS KEY SUPERBASE ELEM
            //                                            // [Compound,Other]
            //                                            // OBJ KEY ELEM
            return false;
        }
        break;
      default:
        break;
    }

    if (!EmitAssignmentRhs(this, rhs, offset))             // ... VAL? RHS
        return false;

    // Assign inferred function name
    if (inferFunctionName) {
        MOZ_ASSERT(!isCompound);
        if (anonFunctionName) {
            // Name is an atom known at compile time
            MOZ_ASSERT_IF(!lhs->isKind(PNK_NAME), isInit);
            if (!setOrEmitSetFunName(rhs, anonFunctionName)) {         // ENV? VAL? RHS
                return false;
            }
        } else if (lhs->isKind(PNK_ELEM)) {
            // offset points to the SP relative to RHS.    // [Simple,Super] offset = 4
            // Find KEY relative to that.                  // {offset} THIS KEY SUPERBASE FUN  
            //                                             // [Simple,Other] offset = 3
            //                                             // {offset} OBJ KEY FUN
            MOZ_ASSERT(offset >= 2);
            if (!emitSetFunctionNameFromStack(offset - 2))
                return false;
        }
    }

    /* If += etc., emit the binary operator with a source note. */
    if (isCompound) {
        if (!newSrcNote(SRC_ASSIGNOP))
            return false;
        if (!emit1(compoundOp))                            // ... VAL
            return false;
    }

    /* Finally, emit the specialized assignment bytecode. */
    switch (lhs->getKind()) {
      case PNK_NAME:  {
        if (!noe->emitAssignment()) {                      // VAL
            return false;
        }

        noe.reset();
        break;
      }
      case PNK_DOT: {
        PropertyAccess* prop = &lhs->as<PropertyAccess>();
        if (!poe->emitAssignment(prop->key().atom())) {    // VAL
            return false;
        }

        poe.reset();
        break;
      }
      case PNK_CALL:
        // We threw above, so nothing to do here.
        break;
      case PNK_ELEM: {
        if (!eoe->emitAssignment()) {                      // VAL
            return false;
        }

        eoe.reset();
        break;
      }
      case PNK_ARRAY:
      case PNK_OBJECT:
        if (!emitDestructuringOps(&lhs->as<ListNode>(), DestructuringAssignment))
            return false;
        break;
      default:
        MOZ_ASSERT(0);
    }
    return true;
}

bool
BytecodeEmitter::emitShortCircuitAssignment(ParseNodeKind kind, JSOp op,
                                            ParseNode* lhs, ParseNode* rhs)
{
    TDZCheckCache tdzCache(this);

    // |name| is used within NameOpEmitter, so its lifetime must surpass |noe|.
    RootedAtom name(cx);

    // Select the appropriate emitter based on the left-hand side.
    Maybe<NameOpEmitter> noe;
    Maybe<PropOpEmitter> poe;
    Maybe<ElemOpEmitter> eoe;

    int32_t startDepth = stackDepth;

    // Number of values pushed onto the stack in addition to the lhs value.
    int32_t numPushed;

    // Evaluate the left-hand side expression and compute any stack values needed
    // for the assignment.
    switch (lhs->getKind()) {
      case PNK_NAME: {
        name = lhs->as<NameNode>().name();
        noe.emplace(this, name, NameOpEmitter::Kind::CompoundAssignment);

        if (!noe->prepareForRhs()) {
            //          [stack] ENV? LHS
            return false;
        }

        numPushed = noe->emittedBindOp();
        break;
      }

    case PNK_DOT: {
        PropertyAccess* prop = &lhs->as<PropertyAccess>();
        bool isSuper = prop->isSuper();

        poe.emplace(this, PropOpEmitter::Kind::CompoundAssignment,
                    isSuper ? PropOpEmitter::ObjKind::Super
                            : PropOpEmitter::ObjKind::Other);

        if (!poe->prepareForObj())
            return false;

        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {
                //        [stack] THIS SUPERBASE
                return false;
            }
        } else {
            if (!emitTree(&prop->expression())) {
                //        [stack] OBJ
                return false;
            }
        }

        if (!poe->emitGet(prop->key().atom())) {
            //          [stack] # if Super
            //          [stack] THIS SUPERBASE LHS
            //          [stack] # otherwise
            //          [stack] OBJ LHS
            return false;
        }

        if (!poe->prepareForRhs()) {
            //          [stack] # if Super
            //          [stack] THIS SUPERBASE LHS
            //          [stack] # otherwise
            //          [stack] OBJ LHS
            return false;
        }

        numPushed = 1 + isSuper;
        break;
      }

    case PNK_ELEM: {
        PropertyByValue* elem = &lhs->as<PropertyByValue>();
        bool isSuper = elem->isSuper();

        eoe.emplace(this, ElemOpEmitter::Kind::CompoundAssignment,
                    isSuper ? ElemOpEmitter::ObjKind::Super
                            : ElemOpEmitter::ObjKind::Other);

        if (!emitElemObjAndKey(elem, isSuper, *eoe)) {
            //          [stack] # if Super
            //          [stack] THIS KEY
            //          [stack] # otherwise
            //          [stack] OBJ KEY
            return false;
        }

        if (!eoe->emitGet()) {
            //          [stack] # if Super
            //          [stack] THIS KEY SUPERBASE LHS
            //          [stack] # otherwise
            //          [stack] OBJ KEY LHS
            return false;
        }

        if (!eoe->prepareForRhs()) {
            //          [stack] # if Super
            //          [stack] THIS KEY SUPERBASE LHS
            //          [stack] # otherwise
            //          [stack] OBJ KEY LHS
            return false;
        }

        numPushed = 2 + isSuper;
        break;
      }

      default:
        MOZ_CRASH();
    }

    MOZ_ASSERT(stackDepth == startDepth + numPushed + 1);

    // Test for the short-circuit condition.
    JumpList jump;
    if (!emitJump(op, &jump)) {
        //              [stack] ... LHS
        return false;
    }

    // The short-circuit condition wasn't fulfilled, pop the left-hand side value
    // which was kept on the stack.
    if (!emit1(JSOP_POP)) {
        //              [stack] ...
        return false;
    }

    // TODO: Open spec issue about setting inferred function names.
    // <https://github.com/tc39/proposal-logical-assignment/issues/23>
    if (!emitTree(rhs)) {
        //              [stack] ... RHS
        return false;
    }

    // Perform the actual assignment.
    switch (lhs->getKind()) {
      case PNK_NAME: {
        if (!noe->emitAssignment()) {
            //          [stack] RHS
            return false;
        }
        break;
      }

      case PNK_DOT: {
        PropertyAccess* prop = &lhs->as<PropertyAccess>();

        if (!poe->emitAssignment(prop->key().atom())) {
            //          [stack] RHS
            return false;
        }
        break;
      }

      case PNK_ELEM: {
        if (!eoe->emitAssignment()) {
            //          [stack] RHS
            return false;
        }
        break;
      }

      default:
        MOZ_CRASH();
    }

    MOZ_ASSERT(stackDepth == startDepth + 1);

    // Join with the short-circuit jump and pop anything left on the stack.
    if (numPushed > 0) {
        if (!newSrcNote(SRC_LOGICASSIGN)) {
            return false;
        }

        JumpList jumpAroundPop;
        if (!emitJump(JSOP_GOTO, &jumpAroundPop)) {
            //            [stack] RHS
            return false;
        }

        if (!emitJumpTargetAndPatch(jump)) {
            //            [stack] ... LHS
            return false;
        }

        // Reconstruct the stack depth after the jump.
        stackDepth = startDepth + 1 + numPushed;

        // Move the left-hand side value to the bottom and pop the rest.
        if (!emitUnpickN(numPushed)) {
            //            [stack] LHS ...
            return false;
        }

        if (!emitPopN(numPushed)) {
            //            [stack] LHS
            return false;
        }

        if (!emitJumpTargetAndPatch(jumpAroundPop)) {
            //            [stack] LHS | RHS
            return false;
        }
    } else {
        if (!emitJumpTargetAndPatch(jump)) {
            //            [stack] LHS | RHS
            return false;
        }
    }

    MOZ_ASSERT(stackDepth == startDepth + 1);

    return true;
}

bool
ParseNode::getConstantValue(ExclusiveContext* cx, AllowConstantObjects allowObjects,
                            MutableHandleValue vp, Value* compare, size_t ncompare,
                            NewObjectKind newKind)
{
    MOZ_ASSERT(newKind == TenuredObject || newKind == SingletonObject);

    switch (getKind()) {
      case PNK_NUMBER:
        vp.setNumber(as<NumericLiteral>().value());
        return true;
      case PNK_BIGINT:
        vp.setBigInt(as<BigIntLiteral>().box()->value());
        return true;
      case PNK_TEMPLATE_STRING:
      case PNK_STRING:
        vp.setString(as<NameNode>().atom());
        return true;
      case PNK_TRUE:
        vp.setBoolean(true);
        return true;
      case PNK_FALSE:
        vp.setBoolean(false);
        return true;
      case PNK_NULL:
        vp.setNull();
        return true;
      case PNK_RAW_UNDEFINED:
        vp.setUndefined();
        return true;
      case PNK_CALLSITEOBJ:
      case PNK_ARRAY: {
        unsigned count;
        ParseNode* pn;

        if (allowObjects == DontAllowObjects) {
            vp.setMagic(JS_GENERIC_MAGIC);
            return true;
        }

        ObjectGroup::NewArrayKind arrayKind = ObjectGroup::NewArrayKind::Normal;
        if (allowObjects == ForCopyOnWriteArray) {
            arrayKind = ObjectGroup::NewArrayKind::CopyOnWrite;
            allowObjects = DontAllowObjects;
        }

        if (getKind() == PNK_CALLSITEOBJ) {
            count = as<CallSiteNode>().count() - 1;
            pn = as<CallSiteNode>().head()->pn_next;
        } else {
            MOZ_ASSERT(isOp(JSOP_NEWINIT) && !as<ListNode>().hasNonConstInitializer());
            count = as<ListNode>().count();
            pn = as<ListNode>().head();
        }

        AutoValueVector values(cx);
        if (!values.appendN(MagicValue(JS_ELEMENTS_HOLE), count))
            return false;
        size_t idx;
        for (idx = 0; pn; idx++, pn = pn->pn_next) {
            if (!pn->getConstantValue(cx, allowObjects, values[idx], values.begin(), idx))
                return false;
            if (values[idx].isMagic(JS_GENERIC_MAGIC)) {
                vp.setMagic(JS_GENERIC_MAGIC);
                return true;
            }
        }
        MOZ_ASSERT(idx == count);

        JSObject* obj = ObjectGroup::newArrayObject(cx, values.begin(), values.length(),
                                                    newKind, arrayKind);
        if (!obj)
            return false;

        if (!CombineArrayElementTypes(cx, obj, compare, ncompare))
            return false;

        vp.setObject(*obj);
        return true;
      }
      case PNK_OBJECT: {
        MOZ_ASSERT(isOp(JSOP_NEWINIT));
        MOZ_ASSERT(!as<ListNode>().hasNonConstInitializer());

        if (allowObjects == DontAllowObjects) {
            vp.setMagic(JS_GENERIC_MAGIC);
            return true;
        }
        MOZ_ASSERT(allowObjects == AllowObjects);

        Rooted<IdValueVector> properties(cx, IdValueVector(cx));

        RootedValue value(cx), idvalue(cx);
        for (ParseNode* item : as<ListNode>().contents()) {
            // MutateProto and Spread, both are unary, cannot appear here.
            BinaryNode* prop = &item->as<BinaryNode>();
            if (!prop->right()->getConstantValue(cx, allowObjects, &value))
                return false;
            if (value.isMagic(JS_GENERIC_MAGIC)) {
                vp.setMagic(JS_GENERIC_MAGIC);
                return true;
            }

            ParseNode* key = prop->left();
            if (key->isKind(PNK_NUMBER)) {
                idvalue = NumberValue(key->as<NumericLiteral>().value());
            } else {
                MOZ_ASSERT(key->isKind(PNK_OBJECT_PROPERTY_NAME) ||
                           key->isKind(PNK_STRING));
                MOZ_ASSERT(key->as<NameNode>().atom() != cx->names().proto);
                idvalue = StringValue(key->as<NameNode>().atom());
            }

            RootedId id(cx);
            if (!ValueToId<CanGC>(cx, idvalue, &id))
                return false;

            if (!properties.append(IdValuePair(id, value)))
                return false;
        }

        JSObject* obj = ObjectGroup::newPlainObject(cx, properties.begin(), properties.length(),
                                                    newKind);
        if (!obj)
            return false;

        if (!CombinePlainObjectPropertyTypes(cx, obj, compare, ncompare))
            return false;

        vp.setObject(*obj);
        return true;
      }
      default:
        MOZ_CRASH("Unexpected node");
    }
    return false;
}

bool
BytecodeEmitter::emitSingletonInitialiser(ParseNode* pn)
{
    NewObjectKind newKind = (pn->getKind() == PNK_OBJECT) ? SingletonObject : TenuredObject;

    RootedValue value(cx);
    if (!pn->getConstantValue(cx, ParseNode::AllowObjects, &value, nullptr, 0, newKind))
        return false;

    MOZ_ASSERT_IF(newKind == SingletonObject, value.toObject().isSingleton());

    ObjectBox* objbox = parser->newObjectBox(&value.toObject());
    if (!objbox)
        return false;

    return emitObjectOp(objbox, JSOP_OBJECT);
}

bool
BytecodeEmitter::emitCallSiteObject(CallSiteNode* callSiteObj)
{
    RootedValue value(cx);
    if (!callSiteObj->getConstantValue(cx, ParseNode::AllowObjects, &value))
        return false;

    MOZ_ASSERT(value.isObject());

    ObjectBox* objbox1 = parser->newObjectBox(&value.toObject());
    if (!objbox1)
        return false;

    if (!callSiteObj->getRawArrayValue(cx, &value))
        return false;

    MOZ_ASSERT(value.isObject());

    ObjectBox* objbox2 = parser->newObjectBox(&value.toObject());
    if (!objbox2)
        return false;

    return emitObjectPairOp(objbox1, objbox2, JSOP_CALLSITEOBJ);
}

/* See the SRC_FOR source note offsetBias comments later in this file. */
JS_STATIC_ASSERT(JSOP_NOP_LENGTH == 1);
JS_STATIC_ASSERT(JSOP_POP_LENGTH == 1);

namespace {

class EmitLevelManager
{
    BytecodeEmitter* bce;
  public:
    explicit EmitLevelManager(BytecodeEmitter* bce) : bce(bce) { bce->emitLevel++; }
    ~EmitLevelManager() { bce->emitLevel--; }
};

} /* anonymous namespace */

bool
BytecodeEmitter::emitCatch(TernaryNode* catchNode)
{
    // We must be nested under a try-finally statement.
    TryFinallyControl& controlInfo = innermostNestableControl->as<TryFinallyControl>();

    /* Pick up the pending exception and bind it to the catch variable. */
    if (!emit1(JSOP_EXCEPTION))
        return false;

    /*
     * Dup the exception object if there is a guard for rethrowing to use
     * it later when rethrowing or in other catches.
     */
    if (catchNode->kid2() && !emit1(JSOP_DUP))
        return false;

    ParseNode* pn2 = catchNode->kid1();
    if (!pn2) {
      // See ES2019 13.15.7 Runtime Semantics: CatchClauseEvaluation
      // Catch variable was omitted: discard the exception.
      if (!emit1(JSOP_POP))
        return false;
    } else {
      switch (pn2->getKind()) {
        case PNK_ARRAY:
        case PNK_OBJECT:
          if (!emitDestructuringOps(&pn2->as<ListNode>(), DestructuringDeclaration))
              return false;
          if (!emit1(JSOP_POP))
              return false;
          break;

        case PNK_NAME:
          if (!emitLexicalInitialization(&pn2->as<NameNode>()))
              return false;
          if (!emit1(JSOP_POP))
              return false;
          break;

        default:
          MOZ_ASSERT(0);
      }
    }

    // If there is a guard expression, emit it and arrange to jump to the next
    // catch block if the guard expression is false.
    if (catchNode->kid2()) {
        if (!emitTree(catchNode->kid2()))
            return false;

        // If the guard expression is false, fall through, pop the block scope,
        // and jump to the next catch block.  Otherwise jump over that code and
        // pop the dupped exception.
        JumpList guardCheck;
        if (!emitJump(JSOP_IFNE, &guardCheck))
            return false;

        {
            NonLocalExitControl nle(this, NonLocalExitControl::Throw);

            // Move exception back to cx->exception to prepare for
            // the next catch.
            if (!emit1(JSOP_THROWING))
                return false;

            // Leave the scope for this catch block.
            if (!nle.prepareForNonLocalJump(&controlInfo))
                return false;

            // Jump to the next handler added by emitTry.
            if (!emitJump(JSOP_GOTO, &controlInfo.guardJump))
                return false;
        }

        // Back to normal control flow.
        if (!emitJumpTargetAndPatch(guardCheck))
            return false;

        // Pop duplicated exception object as we no longer need it.
        if (!emit1(JSOP_POP))
            return false;
    }

    /* Emit the catch body. */
    return emitTree(catchNode->kid3());
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See the
// comment on EmitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitTry(TryNode* tryNode)
{
    ListNode* catchList = tryNode->catchList();
    ParseNode* finallyNode = tryNode->finallyBlock();

    TryEmitter::Kind kind;
    if (catchList) {
        if (finallyNode)
            kind = TryEmitter::TryCatchFinally;
        else
            kind = TryEmitter::TryCatch;
    } else {
        MOZ_ASSERT(finallyNode);
        kind = TryEmitter::TryFinally;
    }
    TryEmitter tryCatch(this, kind);

    if (!tryCatch.emitTry())
        return false;

    if (!emitTree(tryNode->body()))
        return false;

    // If this try has a catch block, emit it.
    if (catchList) {
        // The emitted code for a catch block looks like:
        //
        // [pushlexicalenv]             only if any local aliased
        // exception
        // if there is a catchguard:
        //   dup
        // setlocal 0; pop              assign or possibly destructure exception
        // if there is a catchguard:
        //   < catchguard code >
        //   ifne POST
        //   debugleaveblock
        //   [poplexicalenv]            only if any local aliased
        //   throwing                   pop exception to cx->exception
        //   goto <next catch block>
        //   POST: pop
        // < catch block contents >
        // debugleaveblock
        // [poplexicalenv]              only if any local aliased
        // goto <end of catch blocks>   non-local; finally applies
        //
        // If there's no catch block without a catchguard, the last <next catch
        // block> points to rethrow code.  This code will [gosub] to the finally
        // code if appropriate, and is also used for the catch-all trynote for
        // capturing exceptions thrown from catch{} blocks.
        //
        for (ParseNode* scopeNode : catchList->contents()) {
            LexicalScopeNode* catchScope = &scopeNode->as<LexicalScopeNode>();
            if (!tryCatch.emitCatch())
                return false;

            // Emit the lexical scope and catch body.
            if (!emitTree(catchScope))
                return false;
        }
    }

    // Emit the finally handler, if there is one.
    if (finallyNode) {
        if (!tryCatch.emitFinally(Some(finallyNode->pn_pos.begin)))
            return false;

        if (!emitTree(finallyNode))
            return false;
    }

    if (!tryCatch.emitEnd())
        return false;

    return true;
}

bool
BytecodeEmitter::emitIf(TernaryNode* ifNode)
{
    IfEmitter ifThenElse(this);

  if_again:
    /* Emit code for the condition before pushing stmtInfo. */
    if (!emitTreeInBranch(ifNode->kid1()))
        return false;

    ParseNode* elseNode = ifNode->kid3();
    if (elseNode) {
        if (!ifThenElse.emitThenElse())
            return false;
    } else {
        if (!ifThenElse.emitThen())
            return false;
    }

    /* Emit code for the then part. */
    if (!emitTreeInBranch(ifNode->kid2()))
        return false;

    if (elseNode) {
        if (elseNode->isKind(PNK_IF)) {
            ifNode = &elseNode->as<TernaryNode>();

            if (!ifThenElse.emitElseIf())
                return false;

            goto if_again;
        }

        if (!ifThenElse.emitElse())
            return false;

        /* Emit code for the else part. */
        if (!emitTreeInBranch(elseNode))
            return false;
    }

    if (!ifThenElse.emitEnd())
        return false;

    return true;
}

bool
BytecodeEmitter::emitHoistedFunctionsInList(ListNode* stmtList)
{
    MOZ_ASSERT(stmtList->hasTopLevelFunctionDeclarations());

    for (ParseNode* stmt : stmtList->contents()) {
        ParseNode* maybeFun = stmt;

        if (!sc->strict()) {
            while (maybeFun->isKind(PNK_LABEL))
                maybeFun = maybeFun->as<LabeledStatement>().statement();
        }

        if (maybeFun->is<FunctionNode>() && maybeFun->as<FunctionNode>().functionIsHoisted()) {
            if (!emitTree(maybeFun))
                return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitLexicalScopeBody(ParseNode* body, EmitLineNumberNote emitLineNote)
{
    if (body->isKind(PNK_STATEMENTLIST) &&
        body->as<ListNode>().hasTopLevelFunctionDeclarations())
    {
        // This block contains function statements whose definitions are
        // hoisted to the top of the block. Emit these as a separate pass
        // before the rest of the block.
        if (!emitHoistedFunctionsInList(&body->as<ListNode>()))
            return false;
    }

    // Line notes were updated by emitLexicalScope.
    return emitTree(body, ValueUsage::WantValue, emitLineNote);
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitLexicalScope(LexicalScopeNode* lexicalScope)
{
    LexicalScopeEmitter lse(this);

    ParseNode* body = lexicalScope->scopeBody();
    if (lexicalScope->isEmptyScope()) {
        if (!lse.emitEmptyScope())
            return false;

        if (!emitLexicalScopeBody(body))
            return false;

        if (!lse.emitEnd())
            return false;

        return true;
    }

    // Update line number notes before emitting TDZ poison in
    // EmitterScope::enterLexical to avoid spurious pausing on seemingly
    // non-effectful lines in Debugger.
    //
    // For example, consider the following code.
    //
    // L1: {
    // L2:   let x = 42;
    // L3: }
    //
    // If line number notes were not updated before the TDZ poison, the TDZ
    // poison bytecode sequence of 'uninitialized; initlexical' will have line
    // number L1, and the Debugger will pause there.
    if (!ParseNodeRequiresSpecialLineNumberNotes(body)) {
        ParseNode* pnForPos = body;
        if (body->isKind(PNK_STATEMENTLIST)) {
            if (ParseNode* pn2 = body->as<ListNode>().head()) {
                pnForPos = pn2;
            }
        }
        if (!updateLineNumberNotes(pnForPos->pn_pos.begin))
            return false;
    }

    ScopeKind kind;
    if (body->isKind(PNK_CATCH)) {
        TernaryNode* catchNode = &body->as<TernaryNode>();
        kind = (!catchNode->kid1() || catchNode->kid1()->isKind(PNK_NAME)) ?
               ScopeKind::SimpleCatch :
               ScopeKind::Catch;
    } else
        kind = ScopeKind::Lexical;

    if (!lse.emitScope(kind, lexicalScope->scopeBindings()))
        return false;

    if (body->isKind(PNK_FOR)) {
        // for loops need to emit {FRESHEN,RECREATE}LEXICALENV if there are
        // lexical declarations in the head. Signal this by passing a
        // non-nullptr lexical scope.
        if (!emitFor(&body->as<ForNode>(), &lse.emitterScope()))
            return false;
    } else {
        if (!emitLexicalScopeBody(body, SUPPRESS_LINENOTE))
            return false;
    }

    return lse.emitEnd();
}

bool
BytecodeEmitter::emitWith(BinaryNode* withNode)
{
    // Ensure that the column of the 'with' is set properly.
    if (!updateSourceCoordNotes(withNode->pn_pos.begin)) {
        return false;
    }

    if (!emitTree(withNode->left()))
        return false;

    EmitterScope emitterScope(this);
    if (!emitterScope.enterWith(this))
        return false;

    if (!emitTree(withNode->right()))
        return false;

    return emitterScope.leave(this);
}

bool
BytecodeEmitter::emitRequireObjectCoercible()
{
    // For simplicity, handle this in self-hosted code, at cost of 13 bytes of
    // bytecode versus 1 byte for a dedicated opcode.  As more places need this
    // behavior, we may want to reconsider this tradeoff.

#ifdef DEBUG
    auto depth = this->stackDepth;
#endif
    MOZ_ASSERT(depth > 0);                 // VAL
    if (!emit1(JSOP_DUP))                  // VAL VAL
        return false;

    // Note that "intrinsic" is a misnomer: we're calling a *self-hosted*
    // function that's not an intrinsic!  But it nonetheless works as desired.
    if (!emitAtomOp(cx->names().RequireObjectCoercible,
                    JSOP_GETINTRINSIC))    // VAL VAL REQUIREOBJECTCOERCIBLE
    {
        return false;
    }
    if (!emit1(JSOP_UNDEFINED))            // VAL VAL REQUIREOBJECTCOERCIBLE UNDEFINED
        return false;
    if (!emit2(JSOP_PICK, 2))              // VAL REQUIREOBJECTCOERCIBLE UNDEFINED VAL
        return false;
    if (!emitCall(JSOP_CALL_IGNORES_RV, 1))// VAL IGNORED
        return false;

    if (!emit1(JSOP_POP))                  // VAL
        return false;

    MOZ_ASSERT(depth == this->stackDepth);
    return true;
}

bool
BytecodeEmitter::emitCopyDataProperties(CopyOption option)
{
    DebugOnly<int32_t> depth = this->stackDepth;

    uint32_t argc;
    if (option == CopyOption::Filtered) {
        MOZ_ASSERT(depth > 2);                 // TARGET SOURCE SET
        argc = 3;

        if (!emitAtomOp(cx->names().CopyDataProperties,
                        JSOP_GETINTRINSIC))    // TARGET SOURCE SET COPYDATAPROPERTIES
        {
            return false;
        }
    } else {
        MOZ_ASSERT(depth > 1);                 // TARGET SOURCE
        argc = 2;

        if (!emitAtomOp(cx->names().CopyDataPropertiesUnfiltered,
                        JSOP_GETINTRINSIC))    // TARGET SOURCE COPYDATAPROPERTIES
        {
            return false;
        }
    }

    if (!emit1(JSOP_UNDEFINED))                // TARGET SOURCE *SET COPYDATAPROPERTIES UNDEFINED
        return false;
    if (!emit2(JSOP_PICK, argc + 1))           // SOURCE *SET COPYDATAPROPERTIES UNDEFINED TARGET
        return false;
    if (!emit2(JSOP_PICK, argc + 1))           // *SET COPYDATAPROPERTIES UNDEFINED TARGET SOURCE
        return false;
    if (option == CopyOption::Filtered) {
        if (!emit2(JSOP_PICK, argc + 1))       // COPYDATAPROPERTIES UNDEFINED TARGET SOURCE SET
            return false;
    }
    if (!emitCall(JSOP_CALL_IGNORES_RV, argc)) // IGNORED
        return false;

    if (!emit1(JSOP_POP))                      // -
        return false;

    MOZ_ASSERT(depth - int(argc) == this->stackDepth);
    return true;
}

bool
BytecodeEmitter::emitBigIntOp(BigInt* bigint)
{
    if (!constList.append(BigIntValue(bigint))) {
        return false;
    }
    return emitIndex32(JSOP_BIGINT, constList.length() - 1);
}

bool
BytecodeEmitter::emitIterator()
{
    // Convert iterable to iterator.
    if (!emit1(JSOP_DUP))                                         // OBJ OBJ
        return false;
    if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::iterator)))   // OBJ OBJ @@ITERATOR
        return false;
    if (!emitElemOpBase(JSOP_CALLELEM))                           // OBJ ITERFN
        return false;
    if (!emit1(JSOP_SWAP))                                        // ITERFN OBJ
        return false;
    if (!emitCall(JSOP_CALLITER, 0))                              // ITER
        return false;
    if (!emitCheckIsObj(CheckIsObjectKind::GetIterator))          // ITER
        return false;
    return true;
}

bool
BytecodeEmitter::emitAsyncIterator()
{
    // Convert iterable to iterator.
    if (!emit1(JSOP_DUP))                                         // OBJ OBJ
        return false;
    if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::asyncIterator))) // OBJ OBJ @@ASYNCITERATOR
        return false;
    if (!emitElemOpBase(JSOP_CALLELEM))                           // OBJ ITERFN
        return false;

    InternalIfEmitter ifAsyncIterIsUndefined(this);
    if (!emit1(JSOP_DUP))                                         // OBJ ITERFN ITERFN
        return false;
    if (!emit1(JSOP_UNDEFINED))                                   // OBJ ITERFN ITERFN UNDEF
        return false;
    if (!emit1(JSOP_EQ))                                          // OBJ ITERFN EQ
        return false;
    if (!ifAsyncIterIsUndefined.emitThenElse())                   // OBJ ITERFN
        return false;

    if (!emit1(JSOP_POP))                                         // OBJ
        return false;
    if (!emit1(JSOP_DUP))                                         // OBJ OBJ
        return false;
    if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::iterator)))   // OBJ OBJ @@ITERATOR
        return false;
    if (!emitElemOpBase(JSOP_CALLELEM))                           // OBJ ITERFN
        return false;
    if (!emit1(JSOP_SWAP))                                        // ITERFN OBJ
        return false;
    if (!emitCall(JSOP_CALLITER, 0))                              // ITER
        return false;
    if (!emitCheckIsObj(CheckIsObjectKind::GetIterator))          // ITER
        return false;

    if (!emit1(JSOP_TOASYNCITER))                                 // ITER
        return false;

    if (!ifAsyncIterIsUndefined.emitElse())                       // OBJ ITERFN
        return false;

    if (!emit1(JSOP_SWAP))                                        // ITERFN OBJ
        return false;
    if (!emitCall(JSOP_CALLITER, 0))                              // ITER
        return false;
    if (!emitCheckIsObj(CheckIsObjectKind::GetIterator))          // ITER
        return false;

    if (!ifAsyncIterIsUndefined.emitEnd())                        // ITER
        return false;

    return true;
}

bool
BytecodeEmitter::emitSpread(bool allowSelfHosted)
{
    LoopControl loopInfo(this, StatementKind::Spread);

    // Jump down to the loop condition to minimize overhead assuming at least
    // one iteration, as the other loop forms do.  Annotate so IonMonkey can
    // find the loop-closing jump.
    unsigned noteIndex;
    if (!newSrcNote(SRC_FOR_OF, &noteIndex))
        return false;

    // Jump down to the loop condition to minimize overhead, assuming at least
    // one iteration.  (This is also what we do for loops; whether this
    // assumption holds for spreads is an unanswered question.)
    JumpList initialJump;
    if (!emitJump(JSOP_GOTO, &initialJump))               // ITER ARR I (during the goto)
        return false;

    JumpTarget top{ -1 };
    if (!emitLoopHead(nullptr, &top))                     // ITER ARR I
        return false;

    // When we enter the goto above, we have ITER ARR I on the stack.  But when
    // we reach this point on the loop backedge (if spreading produces at least
    // one value), we've additionally pushed a RESULT iteration value.
    // Increment manually to reflect this.
    this->stackDepth++;

    JumpList beq;
    JumpTarget breakTarget{ -1 };
    {
#ifdef DEBUG
        auto loopDepth = this->stackDepth;
#endif

        // Emit code to assign result.value to the iteration variable.
        if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) // ITER ARR I VALUE
            return false;
        if (!emit1(JSOP_INITELEM_INC))                    // ITER ARR (I+1)
            return false;

        MOZ_ASSERT(this->stackDepth == loopDepth - 1);

        // Spread operations can't contain |continue|, so don't bother setting loop
        // and enclosing "update" offsets, as we do with for-loops.

        // COME FROM the beginning of the loop to here.
        if (!emitLoopEntry(nullptr, initialJump))         // ITER ARR I
            return false;

        if (!emitDupAt(2))                                // ITER ARR I ITER
            return false;
        if (!emitIteratorNext(nullptr, IteratorKind::Sync, allowSelfHosted))  // ITER ARR I RESULT
            return false;
        if (!emit1(JSOP_DUP))                             // ITER ARR I RESULT RESULT
            return false;
        if (!emitAtomOp(cx->names().done, JSOP_GETPROP))  // ITER ARR I RESULT DONE
            return false;

        if (!emitBackwardJump(JSOP_IFEQ, top, &beq, &breakTarget)) // ITER ARR I RESULT
            return false;

        MOZ_ASSERT(this->stackDepth == loopDepth);
    }

    // Let Ion know where the closing jump of this loop is.
    if (!setSrcNoteOffset(noteIndex, 0, beq.offset - initialJump.offset))
        return false;

    // No breaks or continues should occur in spreads.
    MOZ_ASSERT(loopInfo.breaks.offset == -1);
    MOZ_ASSERT(loopInfo.continues.offset == -1);

    if (!tryNoteList.append(JSTRY_FOR_OF, stackDepth, top.offset, breakTarget.offset))
        return false;

    if (!emit2(JSOP_PICK, 3))                             // ARR FINAL_INDEX RESULT ITER
        return false;

    return emitUint16Operand(JSOP_POPN, 2);               // ARR FINAL_INDEX
}

bool
BytecodeEmitter::emitInitializeForInOrOfTarget(TernaryNode* forHead)
{
    MOZ_ASSERT(forHead->isKind(PNK_FORIN) || forHead->isKind(PNK_FOROF));

    MOZ_ASSERT(this->stackDepth >= 1,
               "must have a per-iteration value for initializing");

    ParseNode* target = forHead->kid1();
    MOZ_ASSERT(!forHead->kid2());

    // If the for-in/of loop didn't have a variable declaration, per-loop
    // initialization is just assigning the iteration value to a target
    // expression.
    if (!parser->handler.isDeclarationList(target))
        return emitAssignmentOrInit(PNK_ASSIGN, JSOP_NOP, target, nullptr); // ... ITERVAL

    // Otherwise, per-loop initialization is (possibly) declaration
    // initialization.  If the declaration is a lexical declaration, it must be
    // initialized.  If the declaration is a variable declaration, an
    // assignment to that name (which does *not* necessarily assign to the
    // variable!) must be generated.

    if (!updateSourceCoordNotes(target->pn_pos.begin))
        return false;

    MOZ_ASSERT(target->isForLoopDeclaration());
    target = parser->handler.singleBindingFromDeclaration(&target->as<ListNode>());

    NameNode* nameNode = nullptr;
    if (target->isKind(PNK_NAME)) {
        nameNode = &target->as<NameNode>();
    } else if (target->isKind(PNK_ASSIGN) ||
               target->isKind(PNK_INITPROP)) {
        BinaryNode* assignNode = &target->as<BinaryNode>();
        if (assignNode->left()->is<NameNode>()) {
            nameNode = &assignNode->left()->as<NameNode>();
        }
    }

    if (nameNode) {
        NameOpEmitter noe(this, nameNode->name(), NameOpEmitter::Kind::Initialize);
        if (!noe.prepareForRhs()) {
            return false;
        }
        if (noe.emittedBindOp()) {
            // Per-iteration initialization in for-in/of loops computes the
            // iteration value *before* initializing.  Thus the initializing
            // value may be buried under a bind-specific value on the stack.
            // Swap it to the top of the stack.
            MOZ_ASSERT(stackDepth >= 2);
            if (!emit1(JSOP_SWAP)) {
                return false;
            }
        } else {
             // In cases of emitting a frame slot or environment slot,
             // nothing needs be done.
            MOZ_ASSERT(stackDepth >= 1);
        }
        if (!noe.emitAssignment()) {
            return false;
        }

        // The caller handles removing the iteration value from the stack.
        return true;
    }

    MOZ_ASSERT(!target->isKind(PNK_ASSIGN) && !target->isKind(PNK_INITPROP),
               "for-in/of loop destructuring declarations can't have initializers");

    MOZ_ASSERT(target->isKind(PNK_ARRAY) || target->isKind(PNK_OBJECT));
    return emitDestructuringOps(&target->as<ListNode>(), DestructuringDeclaration);
}

bool
BytecodeEmitter::emitForOf(ForNode* forNode, const EmitterScope* headLexicalEmitterScope)
{
    MOZ_ASSERT(forNode->isKind(PNK_FOR));

    TernaryNode* forOfHead = forNode->head();
    MOZ_ASSERT(forOfHead->isKind(PNK_FOROF));

    unsigned iflags = forNode->iflags();
    IteratorKind iterKind = (iflags & JSITER_FORAWAITOF)
                            ? IteratorKind::Async
                            : IteratorKind::Sync;
    MOZ_ASSERT_IF(iterKind == IteratorKind::Async, sc->asFunctionBox());
    MOZ_ASSERT_IF(iterKind == IteratorKind::Async, sc->asFunctionBox()->isAsync());

    ParseNode* forHeadExpr = forOfHead->kid3();

    // Certain builtins (e.g. Array.from) are implemented in self-hosting
    // as for-of loops.
    bool allowSelfHostedIter = false;
    if (emitterMode == BytecodeEmitter::SelfHosting &&
        forHeadExpr->isKind(PNK_CALL) &&
        forHeadExpr->as<BinaryNode>().left()->name() == cx->names().allowContentIter)
    {
        allowSelfHostedIter = true;
    }

    // Evaluate the expression being iterated.
    if (!emitTree(forHeadExpr))                           // ITERABLE
        return false;
    if (iterKind == IteratorKind::Async) {
        if (!emitAsyncIterator())                         // ITER
            return false;
    } else {
        if (!emitIterator())                              // ITER
            return false;
    }

    int32_t iterDepth = stackDepth;

    // For-of loops have both the iterator, the result, and the result.value
    // on the stack. Push undefineds to balance the stack.
    if (!emit1(JSOP_UNDEFINED))                           // ITER RESULT
        return false;
    if (!emit1(JSOP_UNDEFINED))                           // ITER RESULT UNDEF
        return false;

    ForOfLoopControl loopInfo(this, iterDepth, allowSelfHostedIter, iterKind);

    // Annotate so IonMonkey can find the loop-closing jump.
    unsigned noteIndex;
    if (!newSrcNote(SRC_FOR_OF, &noteIndex))
        return false;

    JumpList initialJump;
    if (!emitJump(JSOP_GOTO, &initialJump))               // ITER RESULT UNDEF
        return false;

    JumpTarget top{ -1 };
    if (!emitLoopHead(nullptr, &top))                     // ITER RESULT UNDEF
        return false;

    // If the loop had an escaping lexical declaration, replace the current
    // environment with an dead zoned one to implement TDZ semantics.
    if (headLexicalEmitterScope) {
        // The environment chain only includes an environment for the for-of
        // loop head *if* a scope binding is captured, thereby requiring
        // recreation each iteration. If a lexical scope exists for the head,
        // it must be the innermost one. If that scope has closed-over
        // bindings inducing an environment, recreate the current environment.
        DebugOnly<ParseNode*> forOfTarget = forOfHead->kid1();
        MOZ_ASSERT(forOfTarget->isKind(PNK_LET) || forOfTarget->isKind(PNK_CONST));
        MOZ_ASSERT(headLexicalEmitterScope == innermostEmitterScope());
        MOZ_ASSERT(headLexicalEmitterScope->scope(this)->kind() == ScopeKind::Lexical);

        if (headLexicalEmitterScope->hasEnvironment()) {
            if (!emit1(JSOP_RECREATELEXICALENV))          // ITER RESULT UNDEF
                return false;
        }

        // For uncaptured bindings, put them back in TDZ.
        if (!headLexicalEmitterScope->deadZoneFrameSlots(this))
            return false;
    }

    JumpList beq;
    JumpTarget breakTarget{ -1 };
    {
#ifdef DEBUG
        auto loopDepth = this->stackDepth;
#endif

        // Emit code to assign result.value to the iteration variable.
        //
        // Note that ES 13.7.5.13, step 5.c says getting result.value does not
        // call IteratorClose, so start JSTRY_ITERCLOSE after the GETPROP.
        if (!emit1(JSOP_POP))                             // ITER RESULT
            return false;
        if (!emit1(JSOP_DUP))                             // ITER RESULT RESULT
            return false;
        if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) // ITER RESULT VALUE
            return false;

        if (!loopInfo.emitBeginCodeNeedingIteratorClose(this))
            return false;

        if (!emitInitializeForInOrOfTarget(forOfHead))    // ITER RESULT VALUE
            return false;

        MOZ_ASSERT(stackDepth == loopDepth,
                   "the stack must be balanced around the initializing "
                   "operation");

        // Remove VALUE from the stack to release it.
        if (!emit1(JSOP_POP))                             // ITER RESULT
            return false;
        if (!emit1(JSOP_UNDEFINED))                       // ITER RESULT UNDEF
            return false;

        // Perform the loop body.
        ParseNode* forBody = forNode->body();
        if (!emitTree(forBody))                           // ITER RESULT UNDEF
            return false;

        MOZ_ASSERT(stackDepth == loopDepth,
                   "the stack must be balanced around the for-of body");

        if (!loopInfo.emitEndCodeNeedingIteratorClose(this))
            return false;

        // Set offset for continues.
        loopInfo.continueTarget = { offset() };

        if (!emitLoopEntry(forHeadExpr, initialJump))     // ITER RESULT UNDEF
            return false;

        if (!emit1(JSOP_SWAP))                            // ITER UNDEF RESULT
            return false;
        if (!emit1(JSOP_POP))                             // ITER UNDEF
            return false;
        if (!emitDupAt(1))                                // ITER UNDEF ITER
            return false;

        if (!emitIteratorNext(forOfHead, iterKind, allowSelfHostedIter)) // ITER UNDEF RESULT
            return false;

        if (!emit1(JSOP_SWAP))                            // ITER RESULT UNDEF
            return false;

        if (!emitDupAt(1))                                // ITER RESULT UNDEF RESULT
            return false;
        if (!emitAtomOp(cx->names().done, JSOP_GETPROP))  // ITER RESULT UNDEF DONE
            return false;

        if (!emitBackwardJump(JSOP_IFEQ, top, &beq, &breakTarget))
            return false;                                 // ITER RESULT UNDEF

        MOZ_ASSERT(this->stackDepth == loopDepth);
    }

    // Let Ion know where the closing jump of this loop is.
    if (!setSrcNoteOffset(noteIndex, 0, beq.offset - initialJump.offset))
        return false;

    if (!loopInfo.patchBreaksAndContinues(this))
        return false;

    if (!tryNoteList.append(JSTRY_FOR_OF, stackDepth, top.offset, breakTarget.offset))
        return false;

    return emitUint16Operand(JSOP_POPN, 3);               //
}

bool
BytecodeEmitter::emitForIn(ForNode* forNode, const EmitterScope* headLexicalEmitterScope)
{
    MOZ_ASSERT(forNode->isKind(PNK_FOR));
    MOZ_ASSERT(forNode->isOp(JSOP_ITER));

    TernaryNode* forInHead = forNode->head();
    MOZ_ASSERT(forInHead->isKind(PNK_FORIN));

    // Annex B: Evaluate the var-initializer expression if present.
    // |for (var i = initializer in expr) { ... }|
    ParseNode* forInTarget = forInHead->kid1();
    if (parser->handler.isDeclarationList(forInTarget)) {
        ParseNode* decl = parser->handler.singleBindingFromDeclaration(&forInTarget->as<ListNode>());
        if (decl->isKind(PNK_NAME)) {
            if (ParseNode* initializer = decl->as<NameNode>().initializer()) {
                MOZ_ASSERT(forInTarget->isKind(PNK_VAR),
                           "for-in initializers are only permitted for |var| declarations");

                if (!updateSourceCoordNotes(decl->pn_pos.begin))
                    return false;

                NameOpEmitter noe(this, decl->name(), NameOpEmitter::Kind::Initialize);
                if (!noe.prepareForRhs()) {
                    return false;
                }
                if (!emitInitializer(initializer, decl)) {
                    return false;
                }
                if (!noe.emitAssignment()) {
                    return false;
                }

                // Pop the initializer.
                if (!emit1(JSOP_POP))
                    return false;
            }
        }
    }

    // Evaluate the expression being iterated.
    ParseNode* expr = forInHead->kid3();
    if (!emitTree(expr))                                  // EXPR
        return false;

    // Convert the value to the appropriate sort of iterator object for the
    // loop variant (for-in, for-each-in, or destructuring for-in).
    unsigned iflags = forNode->iflags();
    MOZ_ASSERT(0 == (iflags & ~(JSITER_FOREACH | JSITER_ENUMERATE)));
    if (!emit2(JSOP_ITER, AssertedCast<uint8_t>(iflags))) // ITER
        return false;

    // For-in loops have both the iterator and the value on the stack. Push
    // undefined to balance the stack.
    if (!emit1(JSOP_UNDEFINED))                           // ITER ITERVAL
        return false;

    LoopControl loopInfo(this, StatementKind::ForInLoop);

    /* Annotate so IonMonkey can find the loop-closing jump. */
    unsigned noteIndex;
    if (!newSrcNote(SRC_FOR_IN, &noteIndex))
        return false;

    // Jump down to the loop condition to minimize overhead (assuming at least
    // one iteration, just like the other loop forms).
    JumpList initialJump;
    if (!emitJump(JSOP_GOTO, &initialJump))               // ITER ITERVAL
        return false;

    JumpTarget top{ -1 };
    if (!emitLoopHead(nullptr, &top))                     // ITER ITERVAL
        return false;

    // If the loop had an escaping lexical declaration, replace the current
    // environment with an dead zoned one to implement TDZ semantics.
    if (headLexicalEmitterScope) {
        // The environment chain only includes an environment for the for-in
        // loop head *if* a scope binding is captured, thereby requiring
        // recreation each iteration. If a lexical scope exists for the head,
        // it must be the innermost one. If that scope has closed-over
        // bindings inducing an environment, recreate the current environment.
        MOZ_ASSERT(forInTarget->isKind(PNK_LET) || forInTarget->isKind(PNK_CONST));
        MOZ_ASSERT(headLexicalEmitterScope == innermostEmitterScope());
        MOZ_ASSERT(headLexicalEmitterScope->scope(this)->kind() == ScopeKind::Lexical);

        if (headLexicalEmitterScope->hasEnvironment()) {
            if (!emit1(JSOP_RECREATELEXICALENV))          // ITER ITERVAL
                return false;
        }

        // For uncaptured bindings, put them back in TDZ.
        if (!headLexicalEmitterScope->deadZoneFrameSlots(this))
            return false;
    }

    {
#ifdef DEBUG
        auto loopDepth = this->stackDepth;
#endif
        MOZ_ASSERT(loopDepth >= 2);

        if (!emitInitializeForInOrOfTarget(forInHead))    // ITER ITERVAL
            return false;

        MOZ_ASSERT(this->stackDepth == loopDepth,
                   "iterator and iterval must be left on the stack");
    }

    // Perform the loop body.
    ParseNode* forBody = forNode->body();
    if (!emitTree(forBody))                               // ITER ITERVAL
        return false;

    // Set offset for continues.
    loopInfo.continueTarget = { offset() };

    if (!emitLoopEntry(nullptr, initialJump))             // ITER ITERVAL
        return false;
    if (!emit1(JSOP_POP))                                 // ITER
        return false;
    if (!emit1(JSOP_MOREITER))                            // ITER NEXTITERVAL?
        return false;
    if (!emit1(JSOP_ISNOITER))                            // ITER NEXTITERVAL? ISNOITER
        return false;

    JumpList beq;
    JumpTarget breakTarget{ -1 };
    if (!emitBackwardJump(JSOP_IFEQ, top, &beq, &breakTarget))
        return false;                                     // ITER NEXTITERVAL

    // Set the srcnote offset so we can find the closing jump.
    if (!setSrcNoteOffset(noteIndex, 0, beq.offset - initialJump.offset))
        return false;

    if (!loopInfo.patchBreaksAndContinues(this))
        return false;

    // Pop the enumeration value.
    if (!emit1(JSOP_POP))                                 // ITER
        return false;

    if (!tryNoteList.append(JSTRY_FOR_IN, this->stackDepth, top.offset, offset()))
        return false;

    return emit1(JSOP_ENDITER);                           //
}

/* C-style `for (init; cond; update) ...` loop. */
bool
BytecodeEmitter::emitCStyleFor(ForNode* forNode, const EmitterScope* headLexicalEmitterScope)
{
    LoopControl loopInfo(this, StatementKind::ForLoop);

    TernaryNode* forHead = forNode->head();
    ParseNode* forBody = forNode->body();

    // If the head of this for-loop declared any lexical variables, the parser
    // wrapped this PNK_FOR node in a PNK_LEXICALSCOPE representing the
    // implicit scope of those variables. By the time we get here, we have
    // already entered that scope. So far, so good.
    //
    // ### Scope freshening
    //
    // Each iteration of a `for (let V...)` loop creates a fresh loop variable
    // binding for V, even if the loop is a C-style `for(;;)` loop:
    //
    //     var funcs = [];
    //     for (let i = 0; i < 2; i++)
    //         funcs.push(function() { return i; });
    //     assertEq(funcs[0](), 0);  // the two closures capture...
    //     assertEq(funcs[1](), 1);  // ...two different `i` bindings
    //
    // This is implemented by "freshening" the implicit block -- changing the
    // scope chain to a fresh clone of the instantaneous block object -- each
    // iteration, just before evaluating the "update" in for(;;) loops.
    //
    // No freshening occurs in `for (const ...;;)` as there's no point: you
    // can't reassign consts. This is observable through the Debugger API. (The
    // ES6 spec also skips cloning the environment in this case.)
    bool forLoopRequiresFreshening = false;
    if (ParseNode* init = forHead->kid1()) {
        // Emit the `init` clause, whether it's an expression or a variable
        // declaration. (The loop variables were hoisted into an enclosing
        // scope, but we still need to emit code for the initializers.)
        if (!updateSourceCoordNotes(init->pn_pos.begin))
            return false;
        if (init->isForLoopDeclaration()) {
            if (!emitTree(init))
                return false;
        } else {
            // 'init' is an expression, not a declaration. emitTree left its
            // value on the stack.
            if (!emitTree(init, ValueUsage::IgnoreValue))
                return false;
            if (!emit1(JSOP_POP))
                return false;
        }

        // ES 13.7.4.8 step 2. The initial freshening.
        //
        // If an initializer let-declaration may be captured during loop iteration,
        // the current scope has an environment.  If so, freshen the current
        // environment to expose distinct bindings for each loop iteration.
        forLoopRequiresFreshening = init->isKind(PNK_LET) && headLexicalEmitterScope;
        if (forLoopRequiresFreshening) {
            // The environment chain only includes an environment for the for(;;)
            // loop head's let-declaration *if* a scope binding is captured, thus
            // requiring a fresh environment each iteration. If a lexical scope
            // exists for the head, it must be the innermost one. If that scope
            // has closed-over bindings inducing an environment, recreate the
            // current environment.
            MOZ_ASSERT(headLexicalEmitterScope == innermostEmitterScope());
            MOZ_ASSERT(headLexicalEmitterScope->scope(this)->kind() == ScopeKind::Lexical);

            if (headLexicalEmitterScope->hasEnvironment()) {
                if (!emit1(JSOP_FRESHENLEXICALENV))
                    return false;
            }
        }
    }

    /*
     * NB: the SRC_FOR note has offsetBias 1 (JSOP_NOP_LENGTH).
     * Use tmp to hold the biased srcnote "top" offset, which differs
     * from the top local variable by the length of the JSOP_GOTO
     * emitted in between tmp and top if this loop has a condition.
     */
    unsigned noteIndex;
    if (!newSrcNote(SRC_FOR, &noteIndex))
        return false;
    if (!emit1(JSOP_NOP))
        return false;
    ptrdiff_t tmp = offset();

    JumpList jmp;
    if (forHead->kid2()) {
        /* Goto the loop condition, which branches back to iterate. */
        if (!emitJump(JSOP_GOTO, &jmp))
            return false;
    }

    /* Emit code for the loop body. */
    JumpTarget top{ -1 };
    if (!emitLoopHead(forBody, &top))
        return false;
    if (jmp.offset == -1 && !emitLoopEntry(forBody, jmp))
        return false;

    if (!emitTreeInBranch(forBody))
        return false;

    // Set loop and enclosing "update" offsets, for continue.  Note that we
    // continue to immediately *before* the block-freshening: continuing must
    // refresh the block.
    if (!emitJumpTarget(&loopInfo.continueTarget))
        return false;

    // ES 13.7.4.8 step 3.e. The per-iteration freshening.
    if (forLoopRequiresFreshening) {
        MOZ_ASSERT(headLexicalEmitterScope == innermostEmitterScope());
        MOZ_ASSERT(headLexicalEmitterScope->scope(this)->kind() == ScopeKind::Lexical);

        if (headLexicalEmitterScope->hasEnvironment()) {
            if (!emit1(JSOP_FRESHENLEXICALENV))
                return false;
        }
    }

    // Check for update code to do before the condition (if any).
    // The update code may not be executed at all; it needs its own TDZ cache.
    if (ParseNode* update = forHead->kid3()) {
        TDZCheckCache tdzCache(this);

        if (!updateSourceCoordNotes(update->pn_pos.begin))
            return false;
        if (!emitTree(update, ValueUsage::IgnoreValue))
            return false;
        if (!emit1(JSOP_POP))
            return false;

        /* Restore the absolute line number for source note readers. */
        uint32_t lineNum = parser->tokenStream.srcCoords.lineNum(forNode->pn_pos.end);
        if (currentLine() != lineNum) {
            if (!newSrcNote2(SRC_SETLINE, ptrdiff_t(lineNum)))
                return false;
            current->currentLine = lineNum;
            current->lastColumn = 0;
        }
    }

    ptrdiff_t tmp3 = offset();

    if (forHead->kid2()) {
        /* Fix up the goto from top to target the loop condition. */
        MOZ_ASSERT(jmp.offset >= 0);
        if (!emitLoopEntry(forHead->kid2(), jmp))
            return false;

        if (!emitTree(forHead->kid2()))
            return false;
    } else if (!forHead->kid3()) {
        // If there is no condition clause and no update clause, mark
        // the loop-ending "goto" with the location of the "for".
        // This ensures that the debugger will stop on each loop
        // iteration.
        if (!updateSourceCoordNotes(forNode->pn_pos.begin))
            return false;
    }

    /* Set the first note offset so we can find the loop condition. */
    if (!setSrcNoteOffset(noteIndex, 0, tmp3 - tmp))
        return false;
    if (!setSrcNoteOffset(noteIndex, 1, loopInfo.continueTarget.offset - tmp))
        return false;

    /* If no loop condition, just emit a loop-closing jump. */
    JumpList beq;
    JumpTarget breakTarget{ -1 };
    if (!emitBackwardJump(forHead->kid2() ? JSOP_IFNE : JSOP_GOTO, top, &beq, &breakTarget))
        return false;

    /* The third note offset helps us find the loop-closing jump. */
    if (!setSrcNoteOffset(noteIndex, 2, beq.offset - tmp))
        return false;

    if (!tryNoteList.append(JSTRY_LOOP, stackDepth, top.offset, breakTarget.offset))
        return false;

    if (!loopInfo.patchBreaksAndContinues(this))
        return false;

    return true;
}

bool
BytecodeEmitter::emitFor(ForNode* forNode, const EmitterScope* headLexicalEmitterScope)
{
    MOZ_ASSERT(forNode->isKind(PNK_FOR));

    if (forNode->head()->isKind(PNK_FORHEAD))
        return emitCStyleFor(forNode, headLexicalEmitterScope);

    if (!updateLineNumberNotes(forNode->pn_pos.begin))
        return false;

    if (forNode->head()->isKind(PNK_FORIN))
        return emitForIn(forNode, headLexicalEmitterScope);

    MOZ_ASSERT(forNode->head()->isKind(PNK_FOROF));
    return emitForOf(forNode, headLexicalEmitterScope);
}

bool
BytecodeEmitter::emitComprehensionForInOrOfVariables(ParseNode* pn, bool* lexicalScope)
{
    // ES6 specifies that lexical for-loop variables get a fresh binding each
    // iteration, and that evaluation of the expression looped over occurs with
    // these variables dead zoned.  But these rules only apply to *standard*
    // for-in/of loops, and we haven't extended these requirements to
    // comprehension syntax.

    *lexicalScope = pn->is<LexicalScopeNode>();
    if (*lexicalScope) {
        // This is initially-ES7-tracked syntax, now with considerably murkier
        // outlook. The scope work is done by the caller by instantiating an
        // EmitterScope. There's nothing to do here.
    } else {
        // This is legacy comprehension syntax.  We'll have PNK_LET here, using
        // a lexical scope provided by/for the entire comprehension.  Name
        // analysis assumes declarations initialize lets, but as we're handling
        // this declaration manually, we must also initialize manually to avoid
        // triggering dead zone checks.
        MOZ_ASSERT(pn->isKind(PNK_LET));
        MOZ_ASSERT(pn->as<ListNode>().count() == 1);

        if (!emitDeclarationList(&pn->as<ListNode>()))
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitComprehensionForOf(ForNode* forNode)
{
    MOZ_ASSERT(forNode->isKind(PNK_COMPREHENSIONFOR));

    TernaryNode* forHead = forNode->head();
    MOZ_ASSERT(forHead->isKind(PNK_FOROF));

    ParseNode* forHeadExpr = forHead->kid3();
    ParseNode* forBody = forNode->body();

    ParseNode* loopDecl = forHead->kid1();
    bool lexicalScope = false;
    if (!emitComprehensionForInOrOfVariables(loopDecl, &lexicalScope))
        return false;

    // For-of loops run with two values on the stack: the iterator and the
    // current result object.

    // Evaluate the expression to the right of 'of'.
    if (!emitTree(forHeadExpr))                // EXPR
        return false;
    if (!emitIterator())                       // ITER
        return false;

    // Push a dummy result so that we properly enter iteration midstream.
    if (!emit1(JSOP_UNDEFINED))                // ITER RESULT
        return false;
    if (!emit1(JSOP_UNDEFINED))                // ITER RESULT VALUE
        return false;

    // Enter the block before the loop body, after evaluating the obj.
    // Initialize let bindings with undefined when entering, as the name
    // assigned to is a plain assignment.
    TDZCheckCache tdzCache(this);
    Maybe<EmitterScope> emitterScope;
    ParseNode* loopVariableName;
    if (lexicalScope) {
        LexicalScopeNode* scopeNode = &loopDecl->as<LexicalScopeNode>();
        loopVariableName = parser->handler.singleBindingFromDeclaration(&scopeNode->scopeBody()->as<ListNode>());
        emitterScope.emplace(this);
        if (!emitterScope->enterComprehensionFor(this, scopeNode->scopeBindings()))
            return false;
    } else {
        loopVariableName = parser->handler.singleBindingFromDeclaration(&loopDecl->as<ListNode>());
    }

    LoopControl loopInfo(this, StatementKind::ForOfLoop);

    // Jump down to the loop condition to minimize overhead assuming at least
    // one iteration, as the other loop forms do.  Annotate so IonMonkey can
    // find the loop-closing jump.
    unsigned noteIndex;
    if (!newSrcNote(SRC_FOR_OF, &noteIndex))
        return false;
    JumpList jmp;
    if (!emitJump(JSOP_GOTO, &jmp))
        return false;

    JumpTarget top{ -1 };
    if (!emitLoopHead(nullptr, &top))
        return false;

#ifdef DEBUG
    int loopDepth = this->stackDepth;
#endif

    // Emit code to assign result.value to the iteration variable.
    if (!emit1(JSOP_POP))                                 // ITER RESULT
        return false;
    if (!emit1(JSOP_DUP))                                 // ITER RESULT RESULT
        return false;
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP))     // ITER RESULT VALUE
        return false;

    // Notice: Comprehension for-of doesn't perform IteratorClose, since it's
    // not in the spec.

    if (!emitAssignmentOrInit(PNK_ASSIGN, JSOP_NOP, loopVariableName, nullptr)) // ITER RESULT VALUE
        return false;

    // Remove VALUE from the stack to release it.
    if (!emit1(JSOP_POP))                                 // ITER RESULT
        return false;
    if (!emit1(JSOP_UNDEFINED))                           // ITER RESULT UNDEF
        return false;

    // The stack should be balanced around the assignment opcode sequence.
    MOZ_ASSERT(this->stackDepth == loopDepth);

    // Emit code for the loop body.
    if (!emitTree(forBody))                               // ITER RESULT UNDEF
        return false;

    // The stack should be balanced around the assignment opcode sequence.
    MOZ_ASSERT(this->stackDepth == loopDepth);

    // Set offset for continues.
    loopInfo.continueTarget = { offset() };

    if (!emitLoopEntry(forHeadExpr, jmp))
        return false;

    if (!emit1(JSOP_SWAP))                                // ITER UNDEF RESULT
        return false;
    if (!emit1(JSOP_POP))                                 // ITER UNDEF
        return false;
    if (!emitDupAt(1))                                    // ITER UNDEF ITER
        return false;
    if (!emitIteratorNext(forHead))                       // ITER UNDEF RESULT
        return false;
    if (!emit1(JSOP_SWAP))                                // ITER RESULT UNDEF
        return false;
    if (!emitDupAt(1))                                    // ITER RESULT UNDEF RESULT
        return false;
    if (!emitAtomOp(cx->names().done, JSOP_GETPROP))      // ITER RESULT UNDEF DONE
        return false;

    JumpList beq;
    JumpTarget breakTarget{ -1 };
    if (!emitBackwardJump(JSOP_IFEQ, top, &beq, &breakTarget)) // ITER RESULT UNDEF
        return false;

    MOZ_ASSERT(this->stackDepth == loopDepth);

    // Let Ion know where the closing jump of this loop is.
    if (!setSrcNoteOffset(noteIndex, 0, beq.offset - jmp.offset))
        return false;

    if (!loopInfo.patchBreaksAndContinues(this))
        return false;

    if (!tryNoteList.append(JSTRY_FOR_OF, stackDepth, top.offset, breakTarget.offset))
        return false;

    if (emitterScope) {
        if (!emitterScope->leave(this))
            return false;
        emitterScope.reset();
    }

    // Pop the result and the iter.
    return emitUint16Operand(JSOP_POPN, 3);               //
}

bool
BytecodeEmitter::emitComprehensionForIn(ForNode* forNode)
{
    MOZ_ASSERT(forNode->isKind(PNK_COMPREHENSIONFOR));

    TernaryNode* forHead = forNode->head();
    MOZ_ASSERT(forHead->isKind(PNK_FORIN));

    ParseNode* forBody = forNode->right();

    ParseNode* loopDecl = forHead->kid1();
    bool lexicalScope = false;
    if (loopDecl && !emitComprehensionForInOrOfVariables(loopDecl, &lexicalScope))
        return false;

    // Evaluate the expression to the right of 'in'.
    if (!emitTree(forHead->kid3()))
        return false;

    /*
     * Emit a bytecode to convert top of stack value to the iterator
     * object depending on the loop variant (for-in, for-each-in, or
     * destructuring for-in).
     */
    MOZ_ASSERT(forNode->isOp(JSOP_ITER));
    if (!emit2(JSOP_ITER, (uint8_t) forNode->iflags()))
        return false;

    // For-in loops have both the iterator and the value on the stack. Push
    // undefined to balance the stack.
    if (!emit1(JSOP_UNDEFINED))
        return false;

    // Enter the block before the loop body, after evaluating the obj.
    // Initialize let bindings with undefined when entering, as the name
    // assigned to is a plain assignment.
    TDZCheckCache tdzCache(this);
    Maybe<EmitterScope> emitterScope;
    if (lexicalScope) {
        emitterScope.emplace(this);
        LexicalScopeNode* scopeNode = &loopDecl->as<LexicalScopeNode>();
        if (!emitterScope->enterComprehensionFor(this, scopeNode->scopeBindings()))
            return false;
    }

    LoopControl loopInfo(this, StatementKind::ForInLoop);

    /* Annotate so IonMonkey can find the loop-closing jump. */
    unsigned noteIndex;
    if (!newSrcNote(SRC_FOR_IN, &noteIndex))
        return false;

    /*
     * Jump down to the loop condition to minimize overhead assuming at
     * least one iteration, as the other loop forms do.
     */
    JumpList jmp;
    if (!emitJump(JSOP_GOTO, &jmp))
        return false;

    JumpTarget top{ -1 };
    if (!emitLoopHead(nullptr, &top))
        return false;

#ifdef DEBUG
    int loopDepth = this->stackDepth;
#endif

    // Emit code to assign the enumeration value to the left hand side, but
    // also leave it on the stack.
    if (!emitAssignmentOrInit(PNK_ASSIGN, JSOP_NOP, forHead->kid2(), nullptr))
        return false;

    /* The stack should be balanced around the assignment opcode sequence. */
    MOZ_ASSERT(this->stackDepth == loopDepth);

    /* Emit code for the loop body. */
    if (!emitTree(forBody))
        return false;

    // Set offset for continues.
    loopInfo.continueTarget = { offset() };

    if (!emitLoopEntry(nullptr, jmp))
        return false;
    if (!emit1(JSOP_POP))
        return false;
    if (!emit1(JSOP_MOREITER))
        return false;
    if (!emit1(JSOP_ISNOITER))
        return false;
    JumpList beq;
    JumpTarget breakTarget{ -1 };
    if (!emitBackwardJump(JSOP_IFEQ, top, &beq, &breakTarget))
        return false;

    /* Set the srcnote offset so we can find the closing jump. */
    if (!setSrcNoteOffset(noteIndex, 0, beq.offset - jmp.offset))
        return false;

    if (!loopInfo.patchBreaksAndContinues(this))
        return false;

    // Pop the enumeration value.
    if (!emit1(JSOP_POP))
        return false;

    JumpTarget endIter{ offset() };
    if (!tryNoteList.append(JSTRY_FOR_IN, this->stackDepth, top.offset, endIter.offset))
        return false;
    if (!emit1(JSOP_ENDITER))
        return false;

    if (emitterScope) {
        if (!emitterScope->leave(this))
            return false;
        emitterScope.reset();
    }

    return true;
}

bool
BytecodeEmitter::emitComprehensionFor(ForNode* forNode)
{
    TernaryNode* head = forNode->head();
    MOZ_ASSERT(head->isKind(PNK_FORIN) ||
               head->isKind(PNK_FOROF));

    if (!updateLineNumberNotes(forNode->pn_pos.begin))
        return false;

    return head->isKind(PNK_FORIN)
           ? emitComprehensionForIn(forNode)
           : emitComprehensionForOf(forNode);
}

MOZ_NEVER_INLINE bool
BytecodeEmitter::emitFunction(FunctionNode* funNode, bool needsProto /* = false */,
                              ListNode* classContentsIfConstructor /* = nullptr */)
{
    FunctionBox* funbox = funNode->funbox();
    RootedFunction fun(cx, funbox->function());

    MOZ_ASSERT((classContentsIfConstructor != nullptr) == (funbox->function()->kind() ==
                                                           JSFunction::FunctionKind::ClassConstructor));
    //                [stack]

    FunctionEmitter fe(this, funbox, funNode->syntaxKind(),
                       funNode->functionIsHoisted());

    /*
     * Set the |wasEmitted| flag in the funbox once the function has been
     * emitted. Function definitions that need hoisting to the top of the
     * function will be seen by emitFunction in two places.
     */
    if (funbox->wasEmitted) {
        if (!fe.emitAgain()) {
            //          [stack]
            return false;
        }

        MOZ_ASSERT_IF(fun->hasScript(), fun->nonLazyScript());
        MOZ_ASSERT(funNode->functionIsHoisted());
        return true;
    }


    if (fun->isInterpreted()) {
        if (fun->isInterpretedLazy()) {
            if (!fe.emitLazy()) {
                //          [stack] FUN?
                return false;
            }

            if (classContentsIfConstructor) {
                fun->lazyScript()->setFieldInitializers(setupFieldInitializers(classContentsIfConstructor,
                                                                               FieldPlacement::Instance));
            }
            return true;
        }

        if (!fe.prepareForNonLazy()) {
            //            [stack]
            return false;
        }

        // Inherit most things (principals, version, etc) from the
        // parent.  Use default values for the rest.
        Rooted<JSScript*> parent(cx, script);
        MOZ_ASSERT(parent->getVersion() == parser->options().version);
        MOZ_ASSERT(parent->mutedErrors() == parser->options().mutedErrors());
        const TransitiveCompileOptions& transitiveOptions = parser->options();
        CompileOptions options(cx, transitiveOptions);

        Rooted<JSObject*> sourceObject(cx, script->sourceObject());
        Rooted<JSScript*> script(cx, JSScript::Create(cx, options, sourceObject,
                                                      funbox->bufStart, funbox->bufEnd,
                                                      funbox->toStringStart,
                                                      funbox->toStringEnd));
        if (!script)
            return false;

        FieldInitializers fieldInitializers = FieldInitializers::Invalid();
        if (classContentsIfConstructor) {
            fieldInitializers = setupFieldInitializers(classContentsIfConstructor, FieldPlacement::Instance);
        }

        BytecodeEmitter bce2(this, parser, funbox, script, /* lazyScript = */ nullptr,
                             funNode->pn_pos, emitterMode, fieldInitializers);
        if (!bce2.init())
            return false;

        /* We measured the max scope depth when we parsed the function. */
        if (!bce2.emitFunctionScript(funNode))
            return false;

        // fieldInitializers are copied to the JSScript inside BytecodeEmitter

        if (funbox->isLikelyConstructorWrapper()) {
            script->setLikelyConstructorWrapper();
        }

        if (!fe.emitNonLazyEnd()) {
            //            [stack] FUN?
            return false;
        }

        return true;
    }

    if (!fe.emitAsmJSModule()) {
        //              [stack]
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitAsyncWrapperLambda(unsigned index, bool isArrow) {
    if (isArrow) {
        if (sc->allowNewTarget()) {
            if (!emit1(JSOP_NEWTARGET))
                return false;
        } else {
            if (!emit1(JSOP_NULL))
                return false;
        }
        if (!emitIndex32(JSOP_LAMBDA_ARROW, index))
            return false;
    } else {
        if (!emitIndex32(JSOP_LAMBDA, index))
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitAsyncWrapper(unsigned index, bool needsHomeObject, bool isArrow,
                                  bool isStarGenerator)
{
    // needsHomeObject can be true for propertyList for extended class.
    // In that case push both unwrapped and wrapped function, in order to
    // initialize home object of unwrapped function, and set wrapped function
    // as a property.
    //
    //   lambda       // unwrapped
    //   dup          // unwrapped unwrapped
    //   toasync      // unwrapped wrapped
    //
    // Emitted code is surrounded by the following code.
    //
    //                    // classObj classCtor classProto
    //   (emitted code)   // classObj classCtor classProto unwrapped wrapped
    //   swap             // classObj classCtor classProto wrapped unwrapped
    //   inithomeobject 1 // classObj classCtor classProto wrapped unwrapped
    //                    //   initialize the home object of unwrapped
    //                    //   with classProto here
    //   pop              // classObj classCtor classProto wrapped
    //   inithiddenprop   // classObj classCtor classProto wrapped
    //                    //   initialize the property of the classProto
    //                    //   with wrapped function here
    //   pop              // classObj classCtor classProto
    //
    // needsHomeObject is false for other cases, push wrapped function only.
    if (!emitAsyncWrapperLambda(index, isArrow))
        return false;
    if (needsHomeObject) {
        if (!emit1(JSOP_DUP))
            return false;
    }
    if (isStarGenerator) {
        if (!emit1(JSOP_TOASYNCGEN))
            return false;
    } else {
        if (!emit1(JSOP_TOASYNC))
            return false;
    }
    return true;
}

bool
BytecodeEmitter::emitDo(BinaryNode* doNode)
{
    ParseNode* bodyNode = doNode->left();
    ParseNode* condNode = doNode->right();

    /* Emit an annotated nop so IonBuilder can recognize the 'do' loop. */
    unsigned noteIndex;
    if (!newSrcNote(SRC_WHILE, &noteIndex))
        return false;
    if (!emit1(JSOP_NOP))
        return false;

    unsigned noteIndex2;
    if (!newSrcNote(SRC_WHILE, &noteIndex2))
        return false;

    /* Compile the loop body. */
    JumpTarget top;
    if (!emitLoopHead(bodyNode, &top))
        return false;

    LoopControl loopInfo(this, StatementKind::DoLoop);

    JumpList empty;
    if (!emitLoopEntry(nullptr, empty))
        return false;

    if (!emitTree(bodyNode))
        return false;

    // Set the offset for continues.
    if (!emitJumpTarget(&loopInfo.continueTarget))
        return false;

    /* Compile the loop condition, now that continues know where to go. */
    if (!emitTree(condNode))
        return false;

    JumpList beq;
    JumpTarget breakTarget{ -1 };
    if (!emitBackwardJump(JSOP_IFNE, top, &beq, &breakTarget))
        return false;

    if (!tryNoteList.append(JSTRY_LOOP, stackDepth, top.offset, breakTarget.offset))
        return false;

    /*
     * Update the annotations with the update and back edge positions, for
     * IonBuilder.
     *
     * Be careful: We must set noteIndex2 before noteIndex in case the noteIndex
     * note gets bigger.
     */
    if (!setSrcNoteOffset(noteIndex2, 0, beq.offset - top.offset))
        return false;
    if (!setSrcNoteOffset(noteIndex, 0, 1 + (loopInfo.continueTarget.offset - top.offset)))
        return false;

    if (!loopInfo.patchBreaksAndContinues(this))
        return false;

    return true;
}

bool
BytecodeEmitter::emitWhile(BinaryNode* whileNode)
{
    /*
     * Minimize bytecodes issued for one or more iterations by jumping to
     * the condition below the body and closing the loop if the condition
     * is true with a backward branch. For iteration count i:
     *
     *  i    test at the top                 test at the bottom
     *  =    ===============                 ==================
     *  0    ifeq-pass                       goto; ifne-fail
     *  1    ifeq-fail; goto; ifne-pass      goto; ifne-pass; ifne-fail
     *  2    2*(ifeq-fail; goto); ifeq-pass  goto; 2*ifne-pass; ifne-fail
     *  . . .
     *  N    N*(ifeq-fail; goto); ifeq-pass  goto; N*ifne-pass; ifne-fail
     */

    // If we have a single-line while, like "while (x) ;", we want to
    // emit the line note before the initial goto, so that the
    // debugger sees a single entry point.  This way, if there is a
    // breakpoint on the line, it will only fire once; and "next"ing
    // will skip the whole loop.  However, for the multi-line case we
    // want to emit the line note after the initial goto, so that
    // "cont" stops on each iteration -- but without a stop before the
    // first iteration.
    if (parser->tokenStream.srcCoords.lineNum(whileNode->pn_pos.begin) ==
        parser->tokenStream.srcCoords.lineNum(whileNode->pn_pos.end) &&
        !updateSourceCoordNotes(whileNode->pn_pos.begin))
        return false;

    ParseNode* bodyNode = whileNode->right();
    ParseNode* condNode = whileNode->left();

    JumpTarget top{ -1 };
    if (!emitJumpTarget(&top))
        return false;

    LoopControl loopInfo(this, StatementKind::WhileLoop);
    loopInfo.continueTarget = top;

    unsigned noteIndex;
    if (!newSrcNote(SRC_WHILE, &noteIndex))
        return false;

    JumpList jmp;
    if (!emitJump(JSOP_GOTO, &jmp))
        return false;

    if (!emitLoopHead(bodyNode, &top))
        return false;

    if (!emitTreeInBranch(bodyNode))
        return false;

    if (!emitLoopEntry(condNode, jmp))
        return false;
    if (!emitTree(condNode))
        return false;

    JumpList beq;
    JumpTarget breakTarget{ -1 };
    if (!emitBackwardJump(JSOP_IFNE, top, &beq, &breakTarget))
        return false;

    if (!tryNoteList.append(JSTRY_LOOP, stackDepth, top.offset, breakTarget.offset))
        return false;

    if (!setSrcNoteOffset(noteIndex, 0, beq.offset - jmp.offset))
        return false;

    if (!loopInfo.patchBreaksAndContinues(this))
        return false;

    return true;
}

bool
BytecodeEmitter::emitBreak(PropertyName* label)
{
    BreakableControl* target;
    SrcNoteType noteType;
    if (label) {
        // Any statement with the matching label may be the break target.
        auto hasSameLabel = [label](LabelControl* labelControl) {
            return labelControl->label() == label;
        };
        target = findInnermostNestableControl<LabelControl>(hasSameLabel);
        noteType = SRC_BREAK2LABEL;
    } else {
        auto isNotLabel = [](BreakableControl* control) {
            return !control->is<LabelControl>();
        };
        target = findInnermostNestableControl<BreakableControl>(isNotLabel);
        noteType = (target->kind() == StatementKind::Switch) ? SRC_SWITCHBREAK : SRC_BREAK;
    }

    return emitGoto(target, &target->breaks, noteType);
}

bool
BytecodeEmitter::emitContinue(PropertyName* label)
{
    LoopControl* target = nullptr;
    if (label) {
        // Find the loop statement enclosed by the matching label.
        NestableControl* control = innermostNestableControl;
        while (!control->is<LabelControl>() || control->as<LabelControl>().label() != label) {
            if (control->is<LoopControl>())
                target = &control->as<LoopControl>();
            control = control->enclosing();
        }
    } else {
        target = findInnermostNestableControl<LoopControl>();
    }
    return emitGoto(target, &target->continues, SRC_CONTINUE);
}

bool
BytecodeEmitter::emitGetFunctionThis(ParseNode* pn)
{
    MOZ_ASSERT(sc->thisBinding() == ThisBinding::Function);
    MOZ_ASSERT(pn->isKind(PNK_NAME));
    MOZ_ASSERT(pn->name() == cx->names().dotThis);

    return emitGetFunctionThis(Some(pn->pn_pos.begin));
}

bool
BytecodeEmitter::emitGetFunctionThis(const mozilla::Maybe<uint32_t>& offset)
{
    if (offset) {
        if (!updateLineNumberNotes(*offset)) {
            return false;
        }
    }

    if (!emitGetName(cx->names().dotThis)) {              // THIS
        return false;
    }
    if (sc->needsThisTDZChecks()) {
        if (!emit1(JSOP_CHECKTHIS)) {                      // THIS
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitGetThisForSuperBase(UnaryNode* superBase)
{
    MOZ_ASSERT(superBase->isKind(PNK_SUPERBASE));
    return emitGetFunctionThis(superBase->kid());          // THIS
}

bool
BytecodeEmitter::emitThisLiteral(ThisLiteral* pn)
{
    if (ParseNode* thisName = pn->kid())
        return emitGetFunctionThis(thisName);              // THIS

    if (sc->thisBinding() == ThisBinding::Module)
        return emit1(JSOP_UNDEFINED);                      // UNDEF

    MOZ_ASSERT(sc->thisBinding() == ThisBinding::Global);
    return emit1(JSOP_GLOBALTHIS);                         // THIS
}

bool
BytecodeEmitter::emitCheckDerivedClassConstructorReturn()
{
    MOZ_ASSERT(lookupName(cx->names().dotThis).hasKnownSlot());
    if (!emitGetName(cx->names().dotThis))
        return false;
    if (!emit1(JSOP_CHECKRETURN))
        return false;
    return true;
}

bool
BytecodeEmitter::emitReturn(UnaryNode* returnNode)
{
    if (!updateSourceCoordNotes(returnNode->pn_pos.begin))
        return false;

    bool needsIteratorResult = sc->isFunctionBox() && sc->asFunctionBox()->needsIteratorResult();
    if (needsIteratorResult) {
        if (!emitPrepareIteratorResult())
            return false;
    }

    /* Push a return value */
    if (ParseNode* expr = returnNode->kid()) {
        if (!emitTree(expr))
            return false;

        bool isAsyncGenerator = sc->asFunctionBox()->isAsync() &&
                                sc->asFunctionBox()->isStarGenerator();
        if (isAsyncGenerator) {
            if (!emitAwaitInInnermostScope())
                return false;
        }
    } else {
        /* No explicit return value provided */
        if (!emit1(JSOP_UNDEFINED))
            return false;
    }

    if (needsIteratorResult) {
        if (!emitFinishIteratorResult(true))
            return false;
    }

    // We know functionBodyEndPos is set because "return" is only
    // valid in a function, and so we've passed through
    // emitFunctionScript.
    if (!updateSourceCoordNotes(*functionBodyEndPos))
        return false;

    /*
     * EmitNonLocalJumpFixup may add fixup bytecode to close open try
     * blocks having finally clauses and to exit intermingled let blocks.
     * We can't simply transfer control flow to our caller in that case,
     * because we must gosub to those finally clauses from inner to outer,
     * with the correct stack pointer (i.e., after popping any with,
     * for/in, etc., slots nested inside the finally's try).
     *
     * In this case we mutate JSOP_RETURN into JSOP_SETRVAL and add an
     * extra JSOP_RETRVAL after the fixups.
     */
    ptrdiff_t top = offset();

    bool needsFinalYield = sc->isFunctionBox() && sc->asFunctionBox()->needsFinalYield();
    bool isDerivedClassConstructor =
        sc->isFunctionBox() && sc->asFunctionBox()->isDerivedClassConstructor();

    if (!emit1((needsFinalYield || isDerivedClassConstructor) ? JSOP_SETRVAL : JSOP_RETURN))
        return false;

    // Make sure that we emit this before popping the blocks in prepareForNonLocalJump,
    // to ensure that the error is thrown while the scope-chain is still intact.
    if (isDerivedClassConstructor) {
        if (!emitCheckDerivedClassConstructorReturn())
            return false;
    }

    NonLocalExitControl nle(this, NonLocalExitControl::Return);

    if (!nle.prepareForNonLocalJumpToOutermost())
        return false;

    if (needsFinalYield) {
        // We know that .generator is on the function scope, as we just exited
        // all nested scopes.
        NameLocation loc =
            *locationOfNameBoundInFunctionScope(cx->names().dotGenerator, varEmitterScope);
        if (!emitGetNameAtLocation(cx->names().dotGenerator, loc))
            return false;
        if (!emitYieldOp(JSOP_FINALYIELDRVAL))
            return false;
    } else if (isDerivedClassConstructor) {
        MOZ_ASSERT(code()[top] == JSOP_SETRVAL);
        if (!emit1(JSOP_RETRVAL))
            return false;
    } else if (top + static_cast<ptrdiff_t>(JSOP_RETURN_LENGTH) != offset()) {
        code()[top] = JSOP_SETRVAL;
        if (!emit1(JSOP_RETRVAL))
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitGetDotGeneratorInScope(EmitterScope& currentScope)
{
    NameLocation loc = *locationOfNameBoundInFunctionScope(cx->names().dotGenerator, &currentScope);
    return emitGetNameAtLocation(cx->names().dotGenerator, loc);
}

bool
BytecodeEmitter::emitInitialYield(UnaryNode* yieldNode)
{
    if (!emitTree(yieldNode->kid()))
        return false;

    if (!emitYieldOp(JSOP_INITIALYIELD))
        return false;

    if (!emit1(JSOP_POP))
        return false;

    return true;
}

bool
BytecodeEmitter::emitYield(UnaryNode* yieldNode)
{
    MOZ_ASSERT(sc->isFunctionBox());
    MOZ_ASSERT(yieldNode->getOp() == JSOP_YIELD);

    bool needsIteratorResult = sc->asFunctionBox()->needsIteratorResult();
    if (needsIteratorResult) {
        if (!emitPrepareIteratorResult())
            return false;
    }

    if (ParseNode* expr = yieldNode->kid()) {
        if (!emitTree(expr))
            return false;
    } else {
        if (!emit1(JSOP_UNDEFINED))
            return false;
    }

    // 11.4.3.7 AsyncGeneratorYield step 5.
    bool isAsyncGenerator = sc->asFunctionBox()->isAsync();
    if (isAsyncGenerator) {
        if (!emitAwaitInInnermostScope())                 // RESULT
            return false;
    }

    if (needsIteratorResult) {
        if (!emitFinishIteratorResult(false))
            return false;
    }

    if (!emitGetDotGeneratorInInnermostScope())
        return false;

    if (!emitYieldOp(JSOP_YIELD))
        return false;

    return true;
}

bool
BytecodeEmitter::emitAwaitInInnermostScope(UnaryNode* awaitNode)
{
    MOZ_ASSERT(sc->isFunctionBox());
    MOZ_ASSERT(awaitNode->getOp() == JSOP_AWAIT);

    if (!emitTree(awaitNode->kid()))
        return false;
    return emitAwaitInInnermostScope();
}

bool
BytecodeEmitter::emitAwaitInScope(EmitterScope& currentScope)
{
    if (!emitGetDotGeneratorInScope(currentScope))
        return false;
    if (!emitYieldOp(JSOP_AWAIT))
        return false;
    return true;
}

bool
BytecodeEmitter::emitYieldStar(ParseNode* iter)
{
    MOZ_ASSERT(sc->isFunctionBox());
    MOZ_ASSERT(sc->asFunctionBox()->isStarGenerator());

    bool isAsyncGenerator = sc->asFunctionBox()->isAsync();

    if (!emitTree(iter))                                  // ITERABLE
        return false;
    if (isAsyncGenerator) {
        if (!emitAsyncIterator())                         // ITER
            return false;
    } else {
        if (!emitIterator())                              // ITER
            return false;
    }

    // Initial send value is undefined.
    if (!emit1(JSOP_UNDEFINED))                           // ITER RECEIVED
        return false;

    int32_t savedDepthTemp;
    int32_t startDepth = stackDepth;
    MOZ_ASSERT(startDepth >= 2);

    TryEmitter tryCatch(this, TryEmitter::TryCatchFinally, TryEmitter::DontUseRetVal,
                        TryEmitter::DontUseControl);
    if (!tryCatch.emitJumpOverCatchAndFinally())          // ITER RESULT
        return false;

    JumpTarget tryStart{ offset() };
    if (!tryCatch.emitTry())                              // ITER RESULT
        return false;

    MOZ_ASSERT(this->stackDepth == startDepth);

    // 11.4.3.7 AsyncGeneratorYield step 5.
    if (isAsyncGenerator) {
        if (!emitAwaitInInnermostScope())                 // NEXT ITER RESULT
            return false;
    }

    // Load the generator object.
    if (!emitGetDotGeneratorInInnermostScope())           // NEXT ITER RESULT GENOBJ
        return false;

    // Yield RESULT as-is, without re-boxing.
    if (!emitYieldOp(JSOP_YIELD))                         // ITER RECEIVED
        return false;

    if (!tryCatch.emitCatch())                            // ITER RESULT
        return false;

    stackDepth = startDepth;                              // ITER RESULT
    if (!emit1(JSOP_EXCEPTION))                           // ITER RESULT EXCEPTION
        return false;
    if (!emitDupAt(2))                                    // ITER RESULT EXCEPTION ITER
        return false;
    if (!emit1(JSOP_DUP))                                 // ITER RESULT EXCEPTION ITER ITER
        return false;
    if (!emitAtomOp(cx->names().throw_, JSOP_CALLPROP))   // ITER RESULT EXCEPTION ITER THROW
        return false;
    if (!emit1(JSOP_DUP))                                 // ITER RESULT EXCEPTION ITER THROW THROW
        return false;
    if (!emit1(JSOP_UNDEFINED))                           // ITER RESULT EXCEPTION ITER THROW THROW UNDEFINED
        return false;
    if (!emit1(JSOP_EQ))                                  // ITER RESULT EXCEPTION ITER THROW ?EQL
        return false;

    InternalIfEmitter ifThrowMethodIsNotDefined(this);
    if (!ifThrowMethodIsNotDefined.emitThen())            // ITER RESULT EXCEPTION ITER THROW
        return false;
    savedDepthTemp = stackDepth;
    if (!emit1(JSOP_POP))                                 // ITER RESULT EXCEPTION ITER
        return false;
    // ES 14.4.13, YieldExpression : yield * AssignmentExpression, step 5.b.iii.2
    //
    // If the iterator does not have a "throw" method, it calls IteratorClose
    // and then throws a TypeError.
    IteratorKind iterKind = isAsyncGenerator ? IteratorKind::Async : IteratorKind::Sync;
    if (!emitIteratorCloseInInnermostScope(iterKind))     // NEXT ITER RESULT EXCEPTION
        return false;
    if (!emitUint16Operand(JSOP_THROWMSG, JSMSG_ITERATOR_NO_THROW)) // throw
        return false;
    stackDepth = savedDepthTemp;
    if (!ifThrowMethodIsNotDefined.emitEnd())             // ITER OLDRESULT EXCEPTION ITER THROW
        return false;
    // ES 14.4.13, YieldExpression : yield * AssignmentExpression, step 5.b.iii.4.
    // RESULT = ITER.throw(EXCEPTION)                     // ITER OLDRESULT EXCEPTION ITER THROW
    if (!emit1(JSOP_SWAP))                                // ITER OLDRESULT EXCEPTION THROW ITER
        return false;
    if (!emit2(JSOP_PICK, 2))                             // ITER OLDRESULT THROW ITER EXCEPTION
        return false;
    if (!emitCall(JSOP_CALL, 1, iter))                    // ITER OLDRESULT RESULT
        return false;

    if (isAsyncGenerator) {
        if (!emitAwaitInInnermostScope())                 // NEXT ITER OLDRESULT RESULT
            return false;
    }

    if (!emitCheckIsObj(CheckIsObjectKind::IteratorThrow)) // ITER OLDRESULT RESULT
        return false;
    if (!emit1(JSOP_SWAP))                                // ITER RESULT OLDRESULT
        return false;
    if (!emit1(JSOP_POP))                                 // ITER RESULT
        return false;
    MOZ_ASSERT(this->stackDepth == startDepth);
    JumpList checkResult;
    // ES 14.4.13, YieldExpression : yield * AssignmentExpression, step 5.b.ii.
    //
    // Note that there is no GOSUB to the finally block here. If the iterator has a
    // "throw" method, it does not perform IteratorClose.
    if (!emitJump(JSOP_GOTO, &checkResult))               // goto checkResult
        return false;

    if (!tryCatch.emitFinally())
         return false;

    // ES 14.4.13, yield * AssignmentExpression, step 5.c
    //
    // Call iterator.return() for receiving a "forced return" completion from
    // the generator.

    InternalIfEmitter ifGeneratorClosing(this);
    if (!emit1(JSOP_ISGENCLOSING))                        // ITER RESULT FTYPE FVALUE CLOSING
        return false;
    if (!ifGeneratorClosing.emitThen())                   // ITER RESULT FTYPE FVALUE
        return false;

    // Step ii.
    //
    // Get the "return" method.
    if (!emitDupAt(3))                                    // ITER RESULT FTYPE FVALUE ITER
        return false;
    if (!emit1(JSOP_DUP))                                 // ITER RESULT FTYPE FVALUE ITER ITER
        return false;
    if (!emitAtomOp(cx->names().return_, JSOP_CALLPROP))  // ITER RESULT FTYPE FVALUE ITER RET
        return false;

    // Step iii.
    //
    // Do nothing if "return" is undefined.
    InternalIfEmitter ifReturnMethodIsDefined(this);
    if (!emit1(JSOP_DUP))                                 // ITER RESULT FTYPE FVALUE ITER RET RET
        return false;
    if (!emit1(JSOP_UNDEFINED))                           // ITER RESULT FTYPE FVALUE ITER RET RET UNDEFINED
        return false;
    if (!emit1(JSOP_NE))                                  // ITER RESULT FTYPE FVALUE ITER RET ?NEQL
        return false;

    // Step iv.
    //
    // Call "return" with the argument passed to Generator.prototype.return,
    // which is currently in rval.value.
    if (!ifReturnMethodIsDefined.emitThenElse())          // ITER OLDRESULT FTYPE FVALUE ITER RET
        return false;
    if (!emit1(JSOP_SWAP))                                // ITER OLDRESULT FTYPE FVALUE RET ITER
        return false;
    if (!emit1(JSOP_GETRVAL))                             // ITER OLDRESULT FTYPE FVALUE RET ITER RVAL
        return false;
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP))     // ITER OLDRESULT FTYPE FVALUE RET ITER VALUE
        return false;
    if (!emitCall(JSOP_CALL, 1))                          // ITER OLDRESULT FTYPE FVALUE RESULT
        return false;

    if (iterKind == IteratorKind::Async) {
        if (!emitAwaitInInnermostScope())                 // ... FTYPE FVALUE RESULT
            return false;
    }

    // Step v.
    if (!emitCheckIsObj(CheckIsObjectKind::IteratorReturn)) // ITER OLDRESULT FTYPE FVALUE RESULT
        return false;

    // Steps vi-viii.
    //
    // Check if the returned object from iterator.return() is done. If not,
    // continuing yielding.
    InternalIfEmitter ifReturnDone(this);
    if (!emit1(JSOP_DUP))                                 // ITER OLDRESULT FTYPE FVALUE RESULT RESULT
        return false;
    if (!emitAtomOp(cx->names().done, JSOP_GETPROP))      // ITER OLDRESULT FTYPE FVALUE RESULT DONE
        return false;
    if (!ifReturnDone.emitThenElse())                     // ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP))     // ITER OLDRESULT FTYPE FVALUE VALUE
        return false;
    if (!emitPrepareIteratorResult())                     // ITER OLDRESULT FTYPE FVALUE VALUE RESULT
        return false;
    if (!emit1(JSOP_SWAP))                                // ITER OLDRESULT FTYPE FVALUE RESULT VALUE
        return false;
    if (!emitFinishIteratorResult(true))                  // ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    if (!emit1(JSOP_SETRVAL))                             // ITER OLDRESULT FTYPE FVALUE
        return false;
    savedDepthTemp = this->stackDepth;
    if (!ifReturnDone.emitElse())                         // ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    if (!emit2(JSOP_UNPICK, 3))                           // ITER RESULT OLDRESULT FTYPE FVALUE
        return false;
    if (!emitUint16Operand(JSOP_POPN, 3))                 // ITER RESULT
        return false;
    {
        // goto tryStart;
        JumpList beq;
        JumpTarget breakTarget{ -1 };
        if (!emitBackwardJump(JSOP_GOTO, tryStart, &beq, &breakTarget)) // ITER RESULT
            return false;
    }
    this->stackDepth = savedDepthTemp;
    if (!ifReturnDone.emitEnd())
        return false;

    if (!ifReturnMethodIsDefined.emitElse())              // ITER RESULT FTYPE FVALUE ITER RET
        return false;
    if (!emit1(JSOP_POP))                                 // ITER RESULT FTYPE FVALUE ITER
        return false;
    if (!emit1(JSOP_POP))                                 // ITER RESULT FTYPE FVALUE
        return false;
    if (!ifReturnMethodIsDefined.emitEnd())
        return false;

    if (!ifGeneratorClosing.emitEnd())
        return false;

    if (!tryCatch.emitEnd())
        return false;

    // After the try-catch-finally block: send the received value to the iterator.
    // result = iter.next(received)                              // ITER RECEIVED
    if (!emit1(JSOP_SWAP))                                       // RECEIVED ITER
        return false;
    if (!emit1(JSOP_DUP))                                        // RECEIVED ITER ITER
        return false;
    if (!emit1(JSOP_DUP))                                        // RECEIVED ITER ITER ITER
        return false;
    if (!emitAtomOp(cx->names().next, JSOP_CALLPROP))            // RECEIVED ITER ITER NEXT
        return false;
    if (!emit1(JSOP_SWAP))                                       // RECEIVED ITER NEXT ITER
        return false;
    if (!emit2(JSOP_PICK, 3))                                    // ITER NEXT ITER RECEIVED
        return false;
    if (!emitCall(JSOP_CALL, 1, iter))                           // ITER RESULT
        return false;

    if (isAsyncGenerator) {
        if (!emitAwaitInInnermostScope())                        // NEXT ITER RESULT RESULT
            return false;
    }

    if (!emitCheckIsObj(CheckIsObjectKind::IteratorNext))        // ITER RESULT
        return false;
    MOZ_ASSERT(this->stackDepth == startDepth);

    if (!emitJumpTargetAndPatch(checkResult))                    // checkResult:
        return false;

    // if (!result.done) goto tryStart;                          // ITER RESULT
    if (!emit1(JSOP_DUP))                                        // ITER RESULT RESULT
        return false;
    if (!emitAtomOp(cx->names().done, JSOP_GETPROP))             // ITER RESULT DONE
        return false;
    // if (!DONE) goto tryStart;
    {
        JumpList beq;
        JumpTarget breakTarget{ -1 };
        if (!emitBackwardJump(JSOP_IFEQ, tryStart, &beq, &breakTarget)) // ITER RESULT
            return false;
    }

    // result.value
    if (!emit1(JSOP_SWAP))                                       // RESULT ITER
        return false;
    if (!emit1(JSOP_POP))                                        // RESULT
        return false;
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP))            // VALUE
        return false;

    MOZ_ASSERT(this->stackDepth == startDepth - 1);

    return true;
}

bool
BytecodeEmitter::emitStatementList(ListNode* stmtList)
{
    for (ParseNode* stmt : stmtList->contents()) {
        if (!emitTree(stmt))
            return false;
    }
    return true;
}

bool
BytecodeEmitter::emitStatement(UnaryNode* exprStmt)
{
    MOZ_ASSERT(exprStmt->isKind(PNK_SEMI));

    ParseNode* expr = exprStmt->kid();
    if (!expr)
        return true;

    if (!updateSourceCoordNotes(exprStmt->pn_pos.begin))
        return false;

    /*
     * Top-level or called-from-a-native JS_Execute/EvaluateScript,
     * debugger, and eval frames may need the value of the ultimate
     * expression statement as the script's result, despite the fact
     * that it appears useless to the compiler.
     *
     * API users may also set the JSOPTION_NO_SCRIPT_RVAL option when
     * calling JS_Compile* to suppress JSOP_SETRVAL.
     */
    bool wantval = false;
    bool useful = false;
    if (sc->isFunctionBox())
        MOZ_ASSERT(!script->noScriptRval());
    else
        useful = wantval = !script->noScriptRval();

    /* Don't eliminate expressions with side effects. */
    if (!useful) {
        if (!checkSideEffects(expr, &useful))
            return false;

        /*
         * Don't eliminate apparently useless expressions if they are labeled
         * expression statements. The startOffset() test catches the case
         * where we are nesting in emitTree for a labeled compound statement.
         */
        if (innermostNestableControl &&
            innermostNestableControl->is<LabelControl>() &&
            innermostNestableControl->as<LabelControl>().startOffset() >= offset())
        {
            useful = true;
        }
    }

    if (useful) {
        JSOp op = wantval ? JSOP_SETRVAL : JSOP_POP;
        ValueUsage valueUsage = wantval ? ValueUsage::WantValue : ValueUsage::IgnoreValue;
        MOZ_ASSERT_IF(expr->isKind(PNK_ASSIGN), expr->isOp(JSOP_NOP));
        if (!emitTree(expr, valueUsage))
            return false;
        if (!emit1(op))
            return false;
    } else if (exprStmt->isDirectivePrologueMember()) {
        // Don't complain about directive prologue members; just don't emit
        // their code.
    } else {
        if (JSAtom* atom = exprStmt->isStringExprStatement()) {
            // Warn if encountering a non-directive prologue member string
            // expression statement, that is inconsistent with the current
            // directive prologue.  That is, a script *not* starting with
            // "use strict" should warn for any "use strict" statements seen
            // later in the script, because such statements are misleading.
            const char* directive = nullptr;
            if (atom == cx->names().useStrict) {
                if (!sc->strictScript)
                    directive = js_useStrict_str;
            } else if (atom == cx->names().useAsm) {
                if (sc->isFunctionBox()) {
                    if (IsAsmJSModule(sc->asFunctionBox()->function()))
                        directive = js_useAsm_str;
                }
            }

            if (directive) {
                if (!reportExtraWarning(expr, JSMSG_CONTRARY_NONDIRECTIVE, directive))
                    return false;
            }
        } else {
            current->currentLine = parser->tokenStream.srcCoords.lineNum(expr->pn_pos.begin);
            current->lastColumn = 0;
            if (!reportExtraWarning(expr, JSMSG_USELESS_EXPR))
                return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitDeleteName(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(PNK_DELETENAME));

    NameNode* nameExpr = &deleteNode->kid()->as<NameNode>();
    MOZ_ASSERT(nameExpr->isKind(PNK_NAME));

    return emitAtomOp(nameExpr->atom(), JSOP_DELNAME);
}

bool
BytecodeEmitter::emitDeleteProperty(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(PNK_DELETEPROP));

    PropertyAccess* propExpr = &deleteNode->kid()->as<PropertyAccess>();
    MOZ_ASSERT(propExpr->isKind(PNK_DOT));

    PropOpEmitter poe(this,
                      PropOpEmitter::Kind::Delete,
                      propExpr->as<PropertyAccess>().isSuper()
                      ? PropOpEmitter::ObjKind::Super
                      : PropOpEmitter::ObjKind::Other);

    if (propExpr->as<PropertyAccess>().isSuper()) {
        // The expression |delete super.foo;| has to evaluate |super.foo|,
        // which could throw if |this| hasn't yet been set by a |super(...)|
        // call or the super-base is not an object, before throwing a
        // ReferenceError for attempting to delete a super-reference.
        UnaryNode* base = &propExpr->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {              // THIS
            return false;
        }
    } else {
        if (!poe.prepareForObj()) {
            return false;
        }
        if (!emitPropLHS(propExpr)) {                      // OBJ
            return false;
        }
    }

    if (!poe.emitDelete(propExpr->key().atom())) {
        //                                                 // [Super]
        //                                                 // THIS
        //                                                 // [Other]
        //                                                 // SUCCEEDED
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDeleteElement(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(PNK_DELETEELEM));

    PropertyByValue* elemExpr = &deleteNode->kid()->as<PropertyByValue>();
    MOZ_ASSERT(elemExpr->isKind(PNK_ELEM));

    bool isSuper = elemExpr->isSuper();
    ElemOpEmitter eoe(this,
                      ElemOpEmitter::Kind::Delete,
                      isSuper
                      ? ElemOpEmitter::ObjKind::Super
                      : ElemOpEmitter::ObjKind::Other);
    if (isSuper) {
        // The expression |delete super[foo];| has to evaluate |super[foo]|,
        // which could throw if |this| hasn't yet been set by a |super(...)|
        // call, or trigger side-effects when evaluating ToPropertyKey(foo),
        // or also throw when the super-base is not an object, before throwing
        // a ReferenceError for attempting to delete a super-reference.
        if (!eoe.prepareForObj()) {                        //
            return false;
        }

        UnaryNode* base = &elemExpr->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {              // THIS
            return false;
        }
        if (!eoe.prepareForKey()) {                        // THIS
            return false;
        }
        if (!emitTree(&elemExpr->key())) {                 // THIS KEY
            return false;
        }
    } else {
        if (!emitElemObjAndKey(elemExpr, false, eoe)) {    // OBJ KEY
            return false;
        }
    }
    if (!eoe.emitDelete()) {                               // [Super]
        //                                                 // THIS
        //                                                 // [Other]
        //                                                 // SUCCEEDED
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDeleteExpression(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(PNK_DELETEEXPR));

    ParseNode* expression = deleteNode->kid();

    // If useless, just emit JSOP_TRUE; otherwise convert |delete <expr>| to
    // effectively |<expr>, true|.
    bool useful = false;
    if (!checkSideEffects(expression, &useful))
        return false;

    if (useful) {
        if (!emitTree(expression))
            return false;
        if (!emit1(JSOP_POP))
            return false;
    }

    return emit1(JSOP_TRUE);
}

bool
BytecodeEmitter::emitDeleteOptionalChain(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(PNK_DELETEOPTCHAIN));

    OptionalEmitter oe(this, stackDepth);

    ParseNode* kid = deleteNode->kid();
    switch (kid->getKind()) {
      case PNK_ELEM:
      case PNK_OPTELEM: {
        PropertyByValueBase* elemExpr = &kid->as<PropertyByValueBase>();
        if (!emitDeleteElementInOptChain(elemExpr, oe)) {
            //              [stack] # If shortcircuit
            //              [stack] UNDEFINED-OR-NULL
            //              [stack] # otherwise
            //              [stack] TRUE
            return false;
        }
        break;
      }
      case PNK_DOT:
      case PNK_OPTDOT: {
        PropertyAccessBase* propExpr = &kid->as<PropertyAccessBase>();
        if (!emitDeletePropertyInOptChain(propExpr, oe)) {
            //              [stack] # If shortcircuit
            //              [stack] UNDEFINED-OR-NULL
            //              [stack] # otherwise
            //              [stack] TRUE
            return false;
        }
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Unrecognized optional delete ParseNodeKind");
    }

    if (!oe.emitOptionalJumpTarget(JSOP_TRUE)) {
        //              [stack] # If shortcircuit
        //              [stack] TRUE
        //              [stack] # otherwise
        //              [stack] SUCCEEDED
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDeletePropertyInOptChain(
    PropertyAccessBase* propExpr,
    OptionalEmitter& oe)
{
    MOZ_ASSERT_IF(propExpr->is<PropertyAccess>(),
                  !propExpr->as<PropertyAccess>().isSuper());

    if (!emitOptionalTree(&propExpr->expression(), oe)) {
        //            [stack] OBJ
        return false;
    }
    if (propExpr->isKind(PNK_OPTDOT)) {
        if (!oe.emitJumpShortCircuit()) {
            //            [stack] # if Jump
            //            [stack] UNDEFINED-OR-NULL
            //            [stack] # otherwise
            //            [stack] OBJ
            return false;
        }
    }

    JSOp delOp = sc->strict() ? JSOP_STRICTDELPROP : JSOP_DELPROP;
    if (!emitAtomOp(propExpr->key().atom(), delOp)) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDeleteElementInOptChain(
    PropertyByValueBase* elemExpr,
    OptionalEmitter& oe)
{
    MOZ_ASSERT_IF(elemExpr->is<PropertyByValue>(),
                  !elemExpr->as<PropertyByValue>().isSuper());

    if (!emitOptionalTree(&elemExpr->expression(), oe)) {
        //              [stack] OBJ
        return false;
    }

    if (elemExpr->isKind(PNK_OPTELEM)) {
        if (!oe.emitJumpShortCircuit()) {
            //            [stack] # if Jump
            //            [stack] UNDEFINED-OR-NULL
            //            [stack] # otherwise
            //            [stack] OBJ
            return false;
        }
    }

    if (!emitTree(&elemExpr->key())) {
        //              [stack] OBJ KEY
        return false;
    }

    JSOp delOp = sc->strict() ? JSOP_STRICTDELELEM : JSOP_DELELEM;
    return emitElemOpBase(delOp);
}

static const char *
SelfHostedCallFunctionName(JSAtom* name, ExclusiveContext* cx)
{
    if (name == cx->names().callFunction)
        return "callFunction";
    if (name == cx->names().callContentFunction)
        return "callContentFunction";
    if (name == cx->names().constructContentFunction)
        return "constructContentFunction";

    MOZ_CRASH("Unknown self-hosted call function name");
}

bool
BytecodeEmitter::emitSelfHostedCallFunction(BinaryNode* callNode)
{
    // Special-casing of callFunction to emit bytecode that directly
    // invokes the callee with the correct |this| object and arguments.
    // callFunction(fun, thisArg, arg0, arg1) thus becomes:
    // - emit lookup for fun
    // - emit lookup for thisArg
    // - emit lookups for arg0, arg1
    //
    // argc is set to the amount of actually emitted args and the
    // emitting of args below is disabled by setting emitArgs to false.
    ParseNode* calleeNode = callNode->left();
    ListNode* argsList = &callNode->right()->as<ListNode>();

    const char* errorName = SelfHostedCallFunctionName(calleeNode->name(), cx);

    if (argsList->count() < 2) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, errorName, "2", "s");
        return false;
    }

    JSOp callOp = callNode->getOp();
    if (callOp != JSOP_CALL) {
        reportError(callNode, JSMSG_NOT_CONSTRUCTOR, errorName);
        return false;
    }

    bool constructing = calleeNode->name() == cx->names().constructContentFunction;
    ParseNode* funNode = argsList->head();
    if (constructing)
        callOp = JSOP_NEW;
    else if (funNode->getKind() == PNK_NAME && funNode->name() == cx->names().std_Function_apply)
        callOp = JSOP_FUNAPPLY;

    if (!emitTree(funNode))
        return false;

#ifdef DEBUG
    if (emitterMode == BytecodeEmitter::SelfHosting &&
        calleeNode->name() == cx->names().callFunction)
    {
        if (!emit1(JSOP_DEBUGCHECKSELFHOSTED))
            return false;
    }
#endif

    ParseNode* thisOrNewTarget = funNode->pn_next;
    if (constructing) {
        // Save off the new.target value, but here emit a proper |this| for a
        // constructing call.
        if (!emit1(JSOP_IS_CONSTRUCTING))
            return false;
    } else {
        // It's |this|, emit it.
        if (!emitTree(thisOrNewTarget))
            return false;
    }

    for (ParseNode* argpn = thisOrNewTarget->pn_next; argpn; argpn = argpn->pn_next) {
        if (!emitTree(argpn))
            return false;
    }

    if (constructing) {
        if (!emitTree(thisOrNewTarget))
            return false;
    }

    uint32_t argc = argsList->count() - 2;
    if (!emitCall(callOp, argc))
        return false;

    return true;
}

bool
BytecodeEmitter::emitSelfHostedResumeGenerator(BinaryNode* callNode)
{
    ListNode* argsList = &callNode->right()->as<ListNode>();

    // Syntax: resumeGenerator(gen, value, 'next'|'throw'|'close')
    if (argsList->count() != 3) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, "resumeGenerator", "1", "s");
        return false;
    }

    ParseNode* genNode = argsList->head();
    if (!emitTree(genNode))
        return false;

    ParseNode* valNode = genNode->pn_next;
    if (!emitTree(valNode))
        return false;

    ParseNode* kindNode = valNode->pn_next;
    MOZ_ASSERT(kindNode->isKind(PNK_STRING));
    uint16_t operand = GeneratorObject::getResumeKind(cx, kindNode->as<NameNode>().atom());
    MOZ_ASSERT(!kindNode->pn_next);

    if (!emitCall(JSOP_RESUME, operand))
        return false;

    return true;
}

bool
BytecodeEmitter::emitSelfHostedForceInterpreter(ParseNode* pn)
{
    if (!emit1(JSOP_FORCEINTERPRETER))
        return false;
    if (!emit1(JSOP_UNDEFINED))
        return false;
    return true;
}

bool
BytecodeEmitter::emitSelfHostedAllowContentIter(BinaryNode* callNode)
{
    ListNode* argsList = &callNode->right()->as<ListNode>();

    if (argsList->count() != 1) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, "allowContentIter", "1", "");
        return false;
    }

    // We're just here as a sentinel. Pass the value through directly.
    return emitTree(argsList->head());
}

bool
BytecodeEmitter::isRestParameter(ParseNode* pn)
{
    if (!sc->isFunctionBox())
        return false;

    FunctionBox* funbox = sc->asFunctionBox();
    RootedFunction fun(cx, funbox->function());
    if (!funbox->hasRest())
        return false;

    if (!pn->isKind(PNK_NAME)) {
        if (emitterMode == BytecodeEmitter::SelfHosting && pn->isKind(PNK_CALL)) {
            BinaryNode* callNode = &pn->as<BinaryNode>();
            ParseNode* calleeNode = callNode->left();
            if (calleeNode->getKind() == PNK_NAME &&
                calleeNode->name() == cx->names().allowContentIter)
                return isRestParameter(callNode->right()->as<ListNode>().head());
        }
        return false;
    }

    JSAtom* name = pn->name();
    Maybe<NameLocation> paramLoc = locationOfNameBoundInFunctionScope(name);
    if (paramLoc && lookupName(name) == *paramLoc) {
        FunctionScope::Data* bindings = funbox->functionScopeBindings();
        if (bindings->nonPositionalFormalStart > 0) {
            // |paramName| can be nullptr when the rest destructuring syntax is
            // used: `function f(...[]) {}`.
            JSAtom* paramName =
                bindings->trailingNames[bindings->nonPositionalFormalStart - 1].name();
            return paramName && name == paramName;
        }
    }

    return false;
}

bool
BytecodeEmitter::emitOptimizeSpread(ParseNode* arg0, JumpList* jmp, bool* emitted)
{
    // Emit a pereparation code to optimize the spread call with a rest
    // parameter:
    //
    //   function f(...args) {
    //     g(...args);
    //   }
    //
    // If the spread operand is a rest parameter and it's optimizable array,
    // skip spread operation and pass it directly to spread call operation.
    // See the comment in OptimizeSpreadCall in Interpreter.cpp for the
    // optimizable conditons.
    if (!isRestParameter(arg0)) {
        *emitted = false;
        return true;
    }

    if (!emitTree(arg0))
        return false;

    if (!emit1(JSOP_OPTIMIZE_SPREADCALL))
        return false;

    if (!emitJump(JSOP_IFNE, jmp))
        return false;

    if (!emit1(JSOP_POP))
        return false;

    *emitted = true;
    return true;
}

/* A version of emitCalleeAndThis for the optional cases:
 *   * a?.()
 *   * a?.b()
 *   * a?.["b"]()
 *   * (a?.b)()
 *
 * See emitCallOrNew and emitOptionalCall for more context.
 */
bool
BytecodeEmitter::emitOptionalCalleeAndThis(
    ParseNode* callNode,
    ParseNode* calleeNode,
    CallOrNewEmitter& cone,
    OptionalEmitter& oe)
{
    JS_CHECK_RECURSION(cx, return false);

    switch (calleeNode->getKind()) {
      case PNK_NAME: {
        if (!cone.emitNameCallee(calleeNode->name())) {    // CALLEE THIS
            return false;
        }
        break;
      }
      case PNK_OPTDOT: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        OptionalPropertyAccess* prop = &calleeNode->as<OptionalPropertyAccess>();
        bool isSuper = false;
        PropOpEmitter& poe = cone.prepareForPropCallee(isSuper);
        if (!emitOptionalDotExpression(prop, poe, isSuper, oe)) {
            return false;
        }
        break;
      }
      case PNK_DOT: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        PropertyAccess* prop = &calleeNode->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        PropOpEmitter& poe = cone.prepareForPropCallee(isSuper);
        if (!emitOptionalDotExpression(prop, poe, isSuper, oe)) {
            return false;
        }
        break;
      }
      case PNK_OPTELEM: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        OptionalPropertyByValue* elem = &calleeNode->as<OptionalPropertyByValue>();
        bool isSuper = false;
        ElemOpEmitter& eoe = cone.prepareForElemCallee(isSuper);
        if (!emitOptionalElemExpression(elem, eoe, isSuper, oe)) {
            return false;
        }
        break;
      }
      case PNK_ELEM: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        PropertyByValue* elem = &calleeNode->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter& eoe = cone.prepareForElemCallee(isSuper);
        if (!emitOptionalElemExpression(elem, eoe, isSuper, oe)) {
            return false;
        }
        break;
      }
      case PNK_FUNCTION: {
        if (!cone.prepareForFunctionCallee()) {
            return false;
        }
        if (!emitOptionalTree(calleeNode, oe)) {
            //          [stack] CALLEE
            return false;
        }
        break;
      }
      case PNK_OPTCHAIN: {
        return emitCalleeAndThisForOptionalChain(&calleeNode->as<UnaryNode>(), callNode, cone);
      }
      default: {
        MOZ_RELEASE_ASSERT(calleeNode->getKind() != PNK_SUPERBASE);
        if (!cone.prepareForOtherCallee()) {
            return false;
        }
        if (!emitOptionalTree(calleeNode, oe)) {
            //          [stack] CALLEE
            return false;
        }
        break;
      }
    }

    if (!cone.emitThis()) {
        return false;
    }

    return true;
}

/*
 * A modified version of emitCallOrNew that handles optional calls.
 *
 * These include the following:
 *    a?.()
 *    a.b?.()
 *    a.["b"]?.()
 *    (a?.b)?.()
 *
 * See CallOrNewEmitter for more context.
 */
bool
BytecodeEmitter::emitOptionalCall(
    BinaryNode* callNode,
    OptionalEmitter& oe,
    ValueUsage valueUsage)
{
    ParseNode* calleeNode = callNode->left();
    ListNode* argsList = &callNode->right()->as<ListNode>();
    bool isCall = true;
    bool isSpread = IsSpreadOp(callNode->getOp());
    uint32_t argc = argsList->count();
    JSOp op = callNode->getOp();

    CallOrNewEmitter cone(this, op,
                          isSpread && (argc == 1) &&
                          isRestParameter(argsList->head()->as<UnaryNode>().kid())
                          ? CallOrNewEmitter::ArgumentsKind::SingleSpreadRest
                          : CallOrNewEmitter::ArgumentsKind::Other,
                          valueUsage);

    if (!emitOptionalCalleeAndThis(callNode, calleeNode, cone, oe)) {
        //              [stack] CALLEE THIS
        return false;
    }

    if (callNode->isKind(PNK_OPTCALL)) {
        if (!oe.emitJumpShortCircuitForCall()) {
            //            [stack] CALLEE THIS
            return false;
        }
    }

    if (!emitArguments(argsList, /* isCall = */ true, isSpread, cone)) {
        //              [stack] CALLEE THIS ARGS...
        return false;
    }

    ParseNode* coordNode = getCoordNode(callNode, calleeNode, argsList);
    if (!cone.emitEnd(argc, Some(coordNode->pn_pos.begin))) {
        //              [stack] RVAL
        return false;
    }

    return true;
}

ParseNode* BytecodeEmitter::getCoordNode(ParseNode* pn,
                                         ParseNode* calleeNode,
                                         ListNode* argsList) {
    ParseNode* coordNode = pn;
    if (pn->isOp(JSOP_CALL) || pn->isOp(JSOP_SPREADCALL) || pn->isOp(JSOP_FUNCALL) ||
        pn->isOp(JSOP_FUNAPPLY)) {
        // Default to using the location of the `(` itself.
        // obj[expr]() // expression
        //          ^  // column coord
        coordNode = argsList;

        switch (calleeNode->getKind()) {
          case PNK_DOT:
            // Use the position of a property access identifier.
            //
            // obj().aprop() // expression
            //       ^       // column coord
            //
            // Note: Because of the constant folding logic in FoldElement,
            // this case also applies for constant string properties.
            //
            // obj()['aprop']() // expression
            //       ^          // column coord
            coordNode = &calleeNode->as<PropertyAccess>().key();
            break;
          case PNK_NAME:
            // Use the start of callee names.
            coordNode = calleeNode;
            break;
          default:
            break;
        }
    }
    return coordNode;
}

bool
BytecodeEmitter::emitArguments(ListNode* argsList, bool isCall, bool isSpread,
                               CallOrNewEmitter& cone)
{
    uint32_t argc = argsList->count();

    if (argc >= ARGC_LIMIT) {
        parser->tokenStream.reportError(isCall
                                        ? JSMSG_TOO_MANY_FUN_ARGS
                                        : JSMSG_TOO_MANY_CON_ARGS);
        return false;
    }
    if (!isSpread) {
        if (!cone.prepareForNonSpreadArguments()) {        // CALLEE THIS
            return false;
        }
        for (ParseNode* arg : argsList->contents()) {
            if (!emitTree(arg)) {
                return false;
            }
        }
    } else {
        if (cone.wantSpreadOperand()) {
            ParseNode* spreadNode = argsList->head();
            if (!emitTree(spreadNode->as<UnaryNode>().kid())) {                     // CALLEE THIS ARG0
                return false;
            }
        }
        if (!cone.emitSpreadArgumentsTest()) {             // CALLEE THIS
            return false;
        }
        if (!emitArray(argsList->head(), argc, JSOP_SPREADCALLARRAY)) {             // CALLEE THIS ARR
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitCallOrNew(
    BinaryNode* callNode,
    ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    /*
     * Emit callable invocation or operator new (constructor call) code.
     * First, emit code for the left operand to evaluate the callable or
     * constructable object expression.
     *
     * For operator new, we emit JSOP_GETPROP instead of JSOP_CALLPROP, etc.
     * This is necessary to interpose the lambda-initialized method read
     * barrier -- see the code in jsinterp.cpp for JSOP_LAMBDA followed by
     * JSOP_{SET,INIT}PROP.
     *
     * Then (or in a call case that has no explicit reference-base
     * object) we emit JSOP_UNDEFINED to produce the undefined |this|
     * value required for calls (which non-strict mode functions
     * will box into the global object).
     */
    bool isCall = callNode->isKind(PNK_CALL) || callNode->isKind(PNK_TAGGED_TEMPLATE);
    ParseNode* calleeNode = callNode->left();
    ListNode* argsList = &callNode->right()->as<ListNode>();

    bool isSpread = IsSpreadOp(callNode->getOp());

    if (calleeNode->isKind(PNK_NAME) &&
        emitterMode == BytecodeEmitter::SelfHosting &&
        !isSpread) {
        // Calls to "forceInterpreter", "callFunction",
        // "callContentFunction", or "resumeGenerator" in self-hosted
        // code generate inline bytecode.
        PropertyName* calleeName = calleeNode->name();
        if (calleeName == cx->names().callFunction ||
            calleeName == cx->names().callContentFunction ||
            calleeName == cx->names().constructContentFunction)
        {
            return emitSelfHostedCallFunction(callNode);
        }
        if (calleeName == cx->names().resumeGenerator)
            return emitSelfHostedResumeGenerator(callNode);
        if (calleeName == cx->names().forceInterpreter)
            return emitSelfHostedForceInterpreter(callNode);
        if (calleeName == cx->names().allowContentIter)
            return emitSelfHostedAllowContentIter(callNode);
        // Fall through.
    }

    uint32_t argc = argsList->count();
    JSOp op = callNode->getOp();
    CallOrNewEmitter cone(this, op,
                          isSpread && (argc == 1) &&
                          isRestParameter(argsList->head()->as<UnaryNode>().kid())
                          ? CallOrNewEmitter::ArgumentsKind::SingleSpreadRest
                          : CallOrNewEmitter::ArgumentsKind::Other,
                          valueUsage);
    if (!emitCalleeAndThis(callNode, calleeNode, cone)) {  // CALLEE THIS
        return false;
    }
    if (!emitArguments(argsList, isCall, isSpread, cone)) {
        return false;                                      // CALLEE THIS ARGS...
    }

    ParseNode* coordNode = getCoordNode(callNode, calleeNode, argsList);

    if (!cone.emitEnd(argc, Some(coordNode->pn_pos.begin))) {
        return false;                                      // RVAL
    }

    return true;
}

bool
BytecodeEmitter::emitCalleeAndThis(
    ParseNode* callNode,
    ParseNode* calleeNode,
    CallOrNewEmitter& cone)
{
    switch (calleeNode->getKind()) {
      case PNK_NAME: {
        if (!cone.emitNameCallee(calleeNode->name())) {    // CALLEE THIS
            return false;
        }
        break;
      }
      case PNK_DOT: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        PropertyAccess* prop = &calleeNode->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        PropOpEmitter& poe = cone.prepareForPropCallee(isSuper);
        if (!poe.prepareForObj()) {
            return false;
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {          // THIS
                return false;
            }
        } else {
            if (!emitPropLHS(prop)) {                      // OBJ
                return false;
            }
        }
        if (!poe.emitGet(prop->key().atom())) {            // CALLEE THIS?
            return false;
        }
        break;
      }
      case PNK_ELEM: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        PropertyByValue* elem = &calleeNode->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter& eoe = cone.prepareForElemCallee(isSuper);
        if (!emitElemObjAndKey(elem, isSuper, eoe)) {      // [Super]
            //                                             // THIS? THIS KEY
            //                                             // [needsThis,Other]
            //                                             // OBJ? OBJ KEY
            return false;
        }
        if (!eoe.emitGet()) {                              // CALLEE? THIS
            return false;
        }
        break;
      }
      case PNK_FUNCTION:
        if (!cone.prepareForFunctionCallee()) {
            return false;
        }
        if (!emitTree(calleeNode)) {                       // CALLEE
            return false;
        }
        break;
      case PNK_SUPERBASE:
        MOZ_ASSERT(callNode->isKind(PNK_SUPERCALL));
        MOZ_ASSERT(parser->handler.isSuperBase(calleeNode));
        if (!cone.emitSuperCallee()) {                     // CALLEE THIS
            return false;
        }
        break;
      case PNK_OPTCHAIN:
        return emitCalleeAndThisForOptionalChain(&calleeNode->as<UnaryNode>(), callNode, cone);
      default:
        if (!cone.prepareForOtherCallee()) {
            return false;
        }
        if (!emitTree(calleeNode)) {
            return false;
        }
        break;
    }

    if (!cone.emitThis()) {                                // CALLEE THIS
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitRightAssociative(ListNode* node)
{
    // ** is the only right-associative operator.
    MOZ_ASSERT(node->isKind(PNK_POW));

    // Right-associative operator chain.
    for (ParseNode* subexpr : node->contents()) {
        if (!emitTree(subexpr))
            return false;
    }
    for (uint32_t i = 0; i < node->count() - 1; i++) {
        if (!emit1(JSOP_POW))
            return false;
    }
    return true;
}

bool
BytecodeEmitter::emitLeftAssociative(ListNode* node)
{
    // Left-associative operator chain.
    JSOp op = node->getOp();
    ParseNode* headExpr = node->head();
    if (op == JSOP_IN && headExpr->isKind(PNK_NAME) && headExpr->as<NameNode>().isPrivateName()) {
        // {Goanna} The only way a "naked" private name can show up as the leftmost side of an in-chain
        //          is from an ergonomic brand check (`this.#x in ...` would be a PNK_DOT child node).
        //          Instead of going through the emitTree machinery, we pretend that this identifier
        //          reference is actually a string, which allows us to use the JSOP_IN interpreter routines.
        //          This erroneously doesn't call updateLineNumberNotes, but this is not a big issue:
        //          the begin pos is correct as we're on the start of the current tree, the end is on the
        //          same line anyway.
        if (!emitAtomOp(headExpr->as<NameNode>().atom(), JSOP_STRING))
            return false;
    } else {
        if (!emitTree(headExpr))
            return false;
    }
    ParseNode* nextExpr = headExpr->pn_next;
    do {
        if (!emitTree(nextExpr))
            return false;
        if (!emit1(op))
            return false;
    } while ((nextExpr = nextExpr->pn_next));
    return true;
}

bool
BytecodeEmitter::emitLogical(ListNode* node)
{
    MOZ_ASSERT(node->isKind(PNK_COALESCE) || node->isKind(PNK_OR) || node->isKind(PNK_AND));

    /*
     * JSOP_OR converts the operand on the stack to boolean, leaves the original
     * value on the stack and jumps if true; otherwise it falls into the next
     * bytecode, which pops the left operand and then evaluates the right operand.
     * The jump goes around the right operand evaluation.
     *
     * JSOP_AND converts the operand on the stack to boolean and jumps if false;
     * otherwise it falls into the right operand's bytecode.
     */

    TDZCheckCache tdzCache(this);

    /* Left-associative operator chain: avoid too much recursion. */
    ParseNode* expr = node->head();
    if (!emitTree(expr))
        return false;
    JSOp op = node->getOp();
    JumpList jump;
    if (!emitJump(op, &jump))
        return false;
    if (!emit1(JSOP_POP))
        return false;

    /* Emit nodes between the head and the tail. */
    while ((expr = expr->pn_next)->pn_next) {
        if (!emitTree(expr))
            return false;
        if (!emitJump(op, &jump))
            return false;
        if (!emit1(JSOP_POP))
            return false;
    }
    if (!emitTree(expr))
        return false;

    if (!emitJumpTargetAndPatch(jump))
        return false;
    return true;
}

bool
BytecodeEmitter::emitSequenceExpr(ListNode* node,
                                  ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    for (ParseNode* child = node->head(); ; child = child->pn_next) {
        if (!updateSourceCoordNotes(child->pn_pos.begin))
            return false;
        if (!emitTree(child, child->pn_next ? ValueUsage::IgnoreValue : valueUsage))
            return false;
        if (!child->pn_next)
            break;
        if (!emit1(JSOP_POP))
            return false;
    }
    return true;
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitIncOrDec(UnaryNode* incDec)
{
    switch (incDec->kid()->getKind()) {
      case PNK_DOT:
        return emitPropIncDec(incDec);
      case PNK_ELEM:
        return emitElemIncDec(incDec);
      case PNK_CALL:
        return emitCallIncDec(incDec);
      default:
        return emitNameIncDec(incDec);
    }

    return true;
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitLabeledStatement(const LabeledStatement* pn)
{
    /*
     * Emit a JSOP_LABEL instruction. The argument is the offset to the statement
     * following the labeled statement.
     */
    uint32_t index;
    if (!makeAtomIndex(pn->label(), &index))
        return false;

    JumpList top;
    if (!emitJump(JSOP_LABEL, &top))
        return false;

    /* Emit code for the labeled statement. */
    LabelControl controlInfo(this, pn->label(), offset());

    if (!emitTree(pn->statement()))
        return false;

    /* Patch the JSOP_LABEL offset. */
    JumpTarget brk{ lastNonJumpTargetOffset() };
    patchJumpsToTarget(top, brk);

    if (!controlInfo.patchBreaks(this))
        return false;

    return true;
}

bool
BytecodeEmitter::emitConditionalExpression(ConditionalExpression& conditional,
                                           ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    /* Emit the condition, then branch if false to the else part. */
    if (!emitTree(&conditional.condition()))
        return false;

    IfEmitter ifThenElse(this);
    if (!ifThenElse.emitCond())
        return false;

    if (!emitTreeInBranch(&conditional.thenExpression(), valueUsage))
        return false;

    if (!ifThenElse.emitElse())
        return false;

    if (!emitTreeInBranch(&conditional.elseExpression(), valueUsage))
        return false;

    if (!ifThenElse.emitEnd())
        return false;
    MOZ_ASSERT(ifThenElse.pushed() == 1);

    return true;
}

bool
BytecodeEmitter::emitPropertyList(ListNode* obj, PropertyEmitter& pe, PropListType type)
{
    //                [stack] CTOR? OBJ

    size_t curFieldKeyIndex = 0;
    size_t curStaticFieldKeyIndex = 0;
    for (ParseNode* propdef : obj->contents()) {
        if (propdef->is<ClassField>()) {
            MOZ_ASSERT(type == ClassBody);
            // Only handle computing field keys here: the .initializers lambda array
            // is created elsewhere.
            ClassField* field = &propdef->as<ClassField>();
            if (field->name().getKind() == PNK_COMPUTED_NAME) {
                HandlePropertyName fieldKeys = field->isStatic() ? cx->names().dotStaticFieldKeys
                                                                 : cx->names().dotFieldKeys;
                if (!emitGetName(fieldKeys)) {
                    //        [stack] CTOR? OBJ ARRAY
                    return false;
                }

                ParseNode* nameExpr = field->name().as<UnaryNode>().kid();

                if (!emitTree(nameExpr)) {
                    //        [stack] ARRAY KEY
                    return false;
                }

                if (!emit1(JSOP_TOID)) {
                    //        [stack] ARRAY KEY
                    return false;
                }

                size_t fieldKeysIndex = field->isStatic() ? curStaticFieldKeyIndex++
                                                          : curFieldKeyIndex++;
                if (!emitUint32Operand(JSOP_INITELEM_ARRAY, fieldKeysIndex)) {
                    //        [stack] ARRAY
                    return false;
                }

                if (!emit1(JSOP_POP)) {
                    //        [stack] CTOR? OBJ
                    return false;
                }
            }
            continue;
        }

        if (propdef->is<StaticClassBlock>()) {
            // Static class blocks are emitted as part of
            // emitCreateFieldInitializers.
            continue;
        }

        if (propdef->is<LexicalScopeNode>()) {
            // Constructors are sometimes wrapped in LexicalScopeNodes. As we already
            // handled emitting the constructor, skip it.
            MOZ_ASSERT(propdef->as<LexicalScopeNode>().scopeBody()->isKind(PNK_CLASSMETHOD));
            continue;
        }

        // Handle __proto__: v specially because *only* this form, and no other
        // involving "__proto__", performs [[Prototype]] mutation.
        if (propdef->isKind(PNK_MUTATEPROTO)) {
            //            [stack] OBJ
            MOZ_ASSERT(type == ObjectLiteral);
            if (!pe.prepareForProtoValue(Some(propdef->pn_pos.begin))) {
                //        [stack] OBJ
                return false;
            }
            if (!emitTree(propdef->as<UnaryNode>().kid())) {
                //        [stack] OBJ PROTO
                return false;
            }
            if (!pe.emitMutateProto()) {
                //        [stack] OBJ
                return false;
            }
            continue;
        }

        if (propdef->isKind(PNK_SPREAD)) {
            MOZ_ASSERT(type == ObjectLiteral);
            //            [stack] OBJ
            if (!pe.prepareForSpreadOperand(Some(propdef->pn_pos.begin))) {
                //        [stack] OBJ OBJ
                return false;
            }
            if (!emitTree(propdef->as<UnaryNode>().kid())) {
                //        [stack] OBJ OBJ VAL
                return false;
            }
            if (!pe.emitSpread()) {
                //        [stack] OBJ
                return false;
            }
            continue;
        }


        BinaryNode* prop = &propdef->as<BinaryNode>();

        ParseNode* key = prop->left();
        ParseNode* propVal = prop->right();
        bool isPropertyAnonFunctionOrClass = propVal->isDirectRHSAnonFunction();
        JSOp op = propdef->getOp();
        MOZ_ASSERT(op == JSOP_INITPROP || op == JSOP_INITPROP_GETTER ||
                   op == JSOP_INITPROP_SETTER);

        auto emitValue = [this, &propVal, &pe]() {
            //            [stack] CTOR? OBJ CTOR? KEY?

            if (!emitTree(propVal)) {
                //          [stack] CTOR? OBJ CTOR? KEY? VAL
                return false;
            }

            if (propVal->is<FunctionNode>() &&
                propVal->as<FunctionNode>().funbox()->needsHomeObject()) {
                FunctionBox* funbox = propVal->as<FunctionNode>().funbox();
                MOZ_ASSERT(funbox->function()->allowSuperProperty());

                if (!pe.emitInitHomeObject(funbox->asyncKind())) {
                    //        [stack] CTOR? OBJ CTOR? KEY? FUN
                    return false;
                }
            }
            return true;
        };

        PropertyEmitter::Kind kind =
            (type == ClassBody && propdef->as<ClassMethod>().isStatic())
                ? PropertyEmitter::Kind::Static
                : PropertyEmitter::Kind::Prototype;

        if (key->isKind(PNK_NUMBER)) {
            //            [stack] CTOR? OBJ
            if (!pe.prepareForIndexPropKey(Some(propdef->pn_pos.begin), kind)) {
                //        [stack] CTOR? OBJ CTOR?
                return false;
            }
            if (!emitNumberOp(key->as<NumericLiteral>().value())) {
                //        [stack] CTOR? OBJ CTOR? KEY
                return false;
            }
            if (!pe.prepareForIndexPropValue()) {
                //        [stack] CTOR? OBJ CTOR? KEY
                return false;
            }
            if (!emitValue()) {
                //        [stack] CTOR? OBJ CTOR? KEY VAL
                return false;
            }

            switch (op) {
              case JSOP_INITPROP:
                if (!pe.emitInitIndexProp(isPropertyAnonFunctionOrClass)) {
                    //    [stack] CTOR? OBJ
                    return false;
                }
                break;
              case JSOP_INITPROP_GETTER:
                MOZ_ASSERT(!isPropertyAnonFunctionOrClass);
                if (!pe.emitInitIndexGetter()) {
                    //    [stack] CTOR? OBJ
                    return false;
                }
                break;
              case JSOP_INITPROP_SETTER:
                MOZ_ASSERT(!isPropertyAnonFunctionOrClass);
                if (!pe.emitInitIndexSetter()) {
                    //    [stack] CTOR? OBJ
                    return false;
                }
                break;
              default:
                MOZ_CRASH("Invalid op");
            }
            continue;
        }

        if (key->isKind(PNK_OBJECT_PROPERTY_NAME) || key->isKind(PNK_STRING)) {
            // EmitClass took care of constructor already.
            if (type == ClassBody && key->as<NameNode>().atom() == cx->names().constructor &&
                !propdef->as<ClassMethod>().isStatic()) {
                continue;
            }

            if (!pe.prepareForPropValue(Some(propdef->pn_pos.begin), kind)) {
                //        [stack] CTOR? OBJ CTOR?
                return false;
            }
            if (!emitValue()) {
                //        [stack] CTOR? OBJ CTOR? VAL
                return false;
            }

            RootedFunction anonFunction(cx);
            if (isPropertyAnonFunctionOrClass) {
                MOZ_ASSERT(op == JSOP_INITPROP);

                if (propVal->is<FunctionNode>()) {
                    // When the value is function, we set the function's name
                    // at the compile-time, instead of emitting SETFUNNAME.
                    FunctionBox* funbox = propVal->as<FunctionNode>().funbox();
                    anonFunction = funbox->function();
                } else {
                    // Only object literal can have a property where key is
                    // name and value is an anonymous class.
                    //
                    //   ({ foo: class {} });
                    MOZ_ASSERT(type == ObjectLiteral);
                    MOZ_ASSERT(propVal->isKind(PNK_CLASS));
                }
            }

            RootedAtom keyAtom(cx, key->as<NameNode>().atom());
            switch (op) {
              case JSOP_INITPROP:
                if (!pe.emitInitProp(keyAtom, isPropertyAnonFunctionOrClass,
                                     anonFunction)) {
                    //      [stack] CTOR? OBJ
                    return false;
                }
                break;
              case JSOP_INITPROP_GETTER:
                MOZ_ASSERT(!isPropertyAnonFunctionOrClass);
                if (!pe.emitInitGetter(keyAtom)) {
                    //      [stack] CTOR? OBJ
                    return false;
                }
              break;
              case JSOP_INITPROP_SETTER:
                MOZ_ASSERT(!isPropertyAnonFunctionOrClass);
                if (!pe.emitInitSetter(keyAtom)) {
                    //      [stack] CTOR? OBJ
                    return false;
                }
              break;
              default: MOZ_CRASH("Invalid op");
            }

            continue;
        }

        MOZ_ASSERT(key->isKind(PNK_COMPUTED_NAME));

        //              [stack] CTOR? OBJ

        if (!pe.prepareForComputedPropKey(Some(propdef->pn_pos.begin), kind)) {
            //          [stack] CTOR? OBJ CTOR?
            return false;
        }
        if (!emitTree(key->as<UnaryNode>().kid())) {
            //          [stack] CTOR? OBJ CTOR? KEY
            return false;
        }
        if (!pe.prepareForComputedPropValue()) {
            //          [stack] CTOR? OBJ CTOR? KEY
            return false;
        }
        if (!emitValue()) {
            //          [stack] CTOR? OBJ CTOR? KEY VAL
            return false;
        }

        switch (op) {
          case JSOP_INITPROP:
            if (!pe.emitInitComputedProp(isPropertyAnonFunctionOrClass)) {
                //      [stack] CTOR? OBJ
                return false;
            }
            break;
          case JSOP_INITPROP_GETTER:
            MOZ_ASSERT(isPropertyAnonFunctionOrClass);
            if (!pe.emitInitComputedGetter()) {
                //      [stack] CTOR? OBJ
                return false;
            }
            break;
          case JSOP_INITPROP_SETTER:
            MOZ_ASSERT(isPropertyAnonFunctionOrClass);
            if (!pe.emitInitComputedSetter()) {
                //      [stack] CTOR? OBJ
                return false;
            }
            break;
          default:
            MOZ_CRASH("Invalid op");
        }
    }

    return true;
}

static bool
HasInitializer(ParseNode* node, bool isStaticContext)
{
    // For the purposes of bytecode emission, StaticClassBlocks are treated as if
    // they were static initializers.
    return (node->is<ClassField>() &&
            node->as<ClassField>().isStatic() == isStaticContext) ||
           (isStaticContext && node->is<StaticClassBlock>());
}

static FunctionNode*
GetInitializer(ParseNode* node, bool isStaticContext)
{
    MOZ_ASSERT(HasInitializer(node, isStaticContext));
    MOZ_ASSERT_IF(!node->is<ClassField>(), isStaticContext);
    return node->is<ClassField>() ? node->as<ClassField>().initializer()
                                  : node->as<StaticClassBlock>().function();
}


FieldInitializers
BytecodeEmitter::setupFieldInitializers(ListNode* classMembers, FieldPlacement placement)
{
    bool isStatic = placement == FieldPlacement::Static;
    size_t numFields = classMembers->count_if([isStatic](ParseNode* propdef) {
        return HasInitializer(propdef, isStatic);
    });

    return FieldInitializers(numFields);
}

// Purpose of .fieldKeys:
// Computed field names (`["x"] = 2;`) must be ran at class-evaluation time, not
// object construction time. The transformation to do so is roughly as follows:
//
// class C {
//   [keyExpr] = valueExpr;
// }
// -->
// let .fieldKeys = [keyExpr];
// let .initializers = [
//   () => {
//     this[.fieldKeys[0]] = valueExpr;
//   }
// ];
// class C {
//   constructor() {
//     .initializers[0]();
//   }
// }
//
// BytecodeEmitter::emitCreateFieldKeys does `let .fieldKeys = [...];`
// BytecodeEmitter::emitPropertyList fills in the elements of the array.
// See Parser::fieldInitializer for the `this[.fieldKeys[0]]` part.
bool
BytecodeEmitter::emitCreateFieldKeys(ListNode* obj, FieldPlacement placement)
{
    bool isStatic = placement == FieldPlacement::Static;
    size_t numFieldKeys = obj->count_if([isStatic](ParseNode* propdef) {
        return propdef->is<ClassField>() &&
               propdef->as<ClassField>().isStatic() == isStatic &&
               propdef->as<ClassField>().name().getKind() == PNK_COMPUTED_NAME;
    });

    if (numFieldKeys == 0)
        return true;

    HandlePropertyName fieldKeys = isStatic ? cx->names().dotStaticFieldKeys
                                            : cx->names().dotFieldKeys;
    NameOpEmitter noe(this, fieldKeys, NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs())
        return false;

    if (!emitUint32Operand(JSOP_NEWARRAY, numFieldKeys)) {
        //            [stack] ARRAY
        return false;
    }

    if (!noe.emitAssignment()) {
        //            [stack] ARRAY
        return false;
    }

    if (!emit1(JSOP_POP)) {
        //            [stack]
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitCreateFieldInitializers(ClassEmitter& ce, ListNode* obj,
                                             FieldPlacement placement)
{
  // FieldPlacement::Instance
  //                [stack] HOMEOBJ HERITAGE?
  //
  // FieldPlacement::Static
  //                [stack] CTOR HOMEOBJ
    FieldInitializers fieldInitializers = setupFieldInitializers(obj, placement);
    MOZ_ASSERT(fieldInitializers.valid);
    size_t numFields = fieldInitializers.numFieldInitializers;

    if (numFields == 0)
        return true;

    bool isStatic = placement == FieldPlacement::Static;
    if (!ce.prepareForFieldInitializers(numFields, isStatic)) {
        //              [stack] HOMEOBJ HERITAGE? ARRAY
        // or:
        //              [stack] CTOR HOMEOBJ ARRAY
        return false;
    }

    for (ParseNode* propdef : obj->contents()) {
        if (!HasInitializer(propdef, isStatic))
            continue;

        FunctionNode* initializer = GetInitializer(propdef, isStatic);
        if (!ce.prepareForFieldInitializer())
            return false;
        if (!emitTree(initializer)) {
            //          [stack] HOMEOBJ HERITAGE? ARRAY LAMBDA
            // or:
            //            [stack] CTOR HOMEOBJ ARRAY LAMBDA
            return false;
        }
        if (initializer->funbox()->needsHomeObject()) {
            MOZ_ASSERT(initializer->funbox()->function()->allowSuperProperty());
            if (!ce.emitFieldInitializerHomeObject(isStatic)) {
                //          [stack] CTOR OBJ ARRAY LAMBDA
                // or:
                //          [stack] CTOR HOMEOBJ ARRAY LAMBDA
                return false;
            }
        }
        if (!ce.emitStoreFieldInitializer()) {
            //          [stack] HOMEOBJ HERITAGE? ARRAY
            // or:
            //            [stack] CTOR HOMEOBJ ARRAY
            return false;
        }
    }

    if (!ce.emitFieldInitializersEnd()) {
        //              [stack] HOMEOBJ HERITAGE?
        // or:
        //              [stack] CTOR HOMEOBJ
        return false;
    }

    return true;
}

const FieldInitializers&
BytecodeEmitter::findFieldInitializersForCall()
{
  for (BytecodeEmitter* current = this; current; current = current->parent) {
      if (current->sc->isFunctionBox()) {
          FunctionBox* box = current->sc->asFunctionBox();
          if (box->function()->kind() == JSFunction::FunctionKind::ClassConstructor) {
              const FieldInitializers& fieldInitializers = current->getFieldInitializers();
              MOZ_ASSERT(fieldInitializers.valid);
              return fieldInitializers;
          }
      }
  }

  for (ScopeIter si(innermostScope()); si; si++) {
      if (si.scope()->is<FunctionScope>()) {
          JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();
          if (fun->kind() == JSFunction::FunctionKind::ClassConstructor) {
              const FieldInitializers& fieldInitializers = fun->isInterpretedLazy()
                                                ? fun->lazyScript()->getFieldInitializers()
                                                : fun->nonLazyScript()->getFieldInitializers();
              MOZ_ASSERT(fieldInitializers.valid);
              return fieldInitializers;
          }
      }
  }

  MOZ_CRASH("Constructor for field initializers not found.");
}

bool
BytecodeEmitter::emitInitializeInstanceFields()
{
    const FieldInitializers& fieldInitializers = findFieldInitializersForCall();
    size_t numFields = fieldInitializers.numFieldInitializers;

    if (numFields == 0) {
      return true;
    }

    if (!emitGetName(cx->names().dotInitializers)) {
        //              [stack] ARRAY
        return false;
    }

    for (size_t fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
      if (fieldIndex < numFields - 1) {
        // We DUP to keep the array around (it is consumed in the bytecode below)
        // for next iterations of this loop, except for the last iteration, which
        // avoids an extra POP at the end of the loop.
        if (!emit1(JSOP_DUP)) {
          //          [stack] ARRAY ARRAY
          return false;
        }
      }

      if (!emitNumberOp(fieldIndex)) {
        //            [stack] ARRAY? ARRAY INDEX
        return false;
      }

      // Don't use CALLELEM here, because the receiver of the call != the receiver
      // of this getelem. (Specifically, the call receiver is `this`, and the
      // receiver of this getelem is `.initializers`)
      if (!emit1(JSOP_GETELEM)) {
        //            [stack] ARRAY? FUNC
        return false;
      }

      // This is guaranteed to run after super(), so we don't need TDZ checks.
      if (!emitGetName(cx->names().dotThis)) {
        //            [stack] ARRAY? FUNC THIS
        return false;
      }

      if (!emitCall(JSOP_CALL_IGNORES_RV, 0)) {
        //            [stack] ARRAY? RVAL
        return false;
      }

      if (!emit1(JSOP_POP)) {
        //            [stack] ARRAY?
        return false;
      }
    }

    return true;
}

bool
BytecodeEmitter::emitInitializeStaticFields(ListNode* classMembers)
{
    size_t numFields = classMembers->count_if([](ParseNode* propdef) {
        return HasInitializer(propdef, true);
    });

    if (numFields == 0) {
      return true;
    }

    if (!emitGetName(cx->names().dotStaticInitializers)) {
        //              [stack] CTOR ARRAY
        return false;
    }

    for (size_t fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
      bool hasNext = fieldIndex < numFields - 1;
      if (fieldIndex < numFields - 1) {
        // We DUP to keep the array around (it is consumed in the bytecode below)
        // for next iterations of this loop, except for the last iteration, which
        // avoids an extra POP at the end of the loop.
        if (!emit1(JSOP_DUP)) {
            //          [stack] CTOR ARRAY ARRAY
            return false;
        }
      }

      if (!emitNumberOp(fieldIndex)) {
          //            [stack] CTOR ARRAY? ARRAY INDEX
          return false;
      }

      // Don't use CALLELEM here, because the receiver of the call != the receiver
      // of this getelem. (Specifically, the call receiver is `ctor`, and the
      // receiver of this getelem is `.staticInitializers`)
      if (!emit1(JSOP_GETELEM)) {
          //            [stack] CTOR ARRAY? FUNC
          return false;
      }

      if (!emitDupAt(1 + hasNext)) {
          //            [stack] CTOR ARRAY? FUNC CTOR
          return false;
      }

      if (!emitCall(JSOP_CALL_IGNORES_RV, 0)) {
          //            [stack] CTOR ARRAY? RVAL
          return false;
      }

      if (!emit1(JSOP_POP)) {
          //            [stack] CTOR ARRAY?
          return false;
      }
    }

    // Overwrite |.staticInitializers| and |.staticFieldKeys| with undefined to
    // avoid keeping the arrays alive indefinitely.
    auto clearStaticFieldSlot = [&](HandlePropertyName name) {
        NameOpEmitter noe(this, name, NameOpEmitter::Kind::SimpleAssignment);
        if (!noe.prepareForRhs()) {
            //            [stack] ENV? VAL?
            return false;
        }

        if (!emit1(JSOP_UNDEFINED)) {
            //            [stack] ENV? VAL? UNDEFINED
            return false;
        }

        if (!noe.emitAssignment()) {
            //            [stack] VAL
            return false;
        }

        if (!emit1(JSOP_POP)) {
            //            [stack]
            return false;
        }

        return true;
    };

    if (!clearStaticFieldSlot(cx->names().dotStaticInitializers))
        return false;

    auto isStaticFieldWithComputedName = [](ParseNode* propdef) {
        return propdef->is<ClassField>() &&
               propdef->as<ClassField>().isStatic() &&
               propdef->as<ClassField>().name().getKind() == PNK_COMPUTED_NAME;
    };

    if (classMembers->any_of(isStaticFieldWithComputedName)) {
        if (!clearStaticFieldSlot(cx->names().dotStaticFieldKeys))
            return false;
    }

    return true;
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitObject(ListNode* objNode)
{
    if (!objNode->hasNonConstInitializer() && objNode->head() && checkSingletonContext())
        return emitSingletonInitialiser(objNode);

    //                [stack]

    ObjectEmitter oe(this);
    if (!oe.emitObject(objNode->count())) {
        //            [stack] OBJ
        return false;
    }
    if (!emitPropertyList(objNode, oe, ObjectLiteral)) {
        //            [stack] OBJ
        return false;
    }
    if (!oe.emitEnd()) {
        //            [stack] OBJ
        return false;
    }
    return true;
}

bool
BytecodeEmitter::replaceNewInitWithNewObject(JSObject* obj, ptrdiff_t offset)
{
    ObjectBox* objbox = parser->newObjectBox(obj);
    if (!objbox)
        return false;

    static_assert(JSOP_NEWINIT_LENGTH == JSOP_NEWOBJECT_LENGTH,
                  "newinit and newobject must have equal length to edit in-place");

    uint32_t index = objectList.add(objbox);
    jsbytecode* code = this->code(offset);

    MOZ_ASSERT(code[0] == JSOP_NEWINIT);
    code[0] = JSOP_NEWOBJECT;
    code[1] = jsbytecode(index >> 24);
    code[2] = jsbytecode(index >> 16);
    code[3] = jsbytecode(index >> 8);
    code[4] = jsbytecode(index);

    return true;
}

bool
BytecodeEmitter::emitArrayComp(ListNode* pn)
{
    if (!emitNewInit(JSProto_Array))
        return false;

    /*
     * Pass the new array's stack index to the PNK_ARRAYPUSH case via
     * arrayCompDepth, then simply traverse the PNK_FOR node and
     * its kids under pn2 to generate this comprehension.
     */
    MOZ_ASSERT(stackDepth > 0);
    uint32_t saveDepth = arrayCompDepth;
    arrayCompDepth = (uint32_t) (stackDepth - 1);
    if (!emitTree(pn->head()))
        return false;
    arrayCompDepth = saveDepth;

    return true;
}

bool
BytecodeEmitter::emitArrayLiteral(ListNode* array)
{
    if (!array->hasNonConstInitializer() && array->head()) {
        if (checkSingletonContext()) {
            // Bake in the object entirely if it will only be created once.
            return emitSingletonInitialiser(array);
        }

        // If the array consists entirely of primitive values, make a
        // template object with copy on write elements that can be reused
        // every time the initializer executes. Don't do this if the array is
        // small: copying the elements lazily is not worth it in that case.
        static const size_t MinElementsForCopyOnWrite = 5;
        if (emitterMode != BytecodeEmitter::SelfHosting &&
            array->count() >= MinElementsForCopyOnWrite) {
            RootedValue value(cx);
            if (!array->getConstantValue(cx, ParseNode::ForCopyOnWriteArray, &value))
                return false;
            if (!value.isMagic(JS_GENERIC_MAGIC)) {
                // Note: the group of the template object might not yet reflect
                // that the object has copy on write elements. When the
                // interpreter or JIT compiler fetches the template, it should
                // use ObjectGroup::getOrFixupCopyOnWriteObject to make sure the
                // group for the template is accurate. We don't do this here as we
                // want to use ObjectGroup::allocationSiteGroup, which requires a
                // finished script.
                JSObject* obj = &value.toObject();
                MOZ_ASSERT(obj->is<ArrayObject>() &&
                           obj->as<ArrayObject>().denseElementsAreCopyOnWrite());

                ObjectBox* objbox = parser->newObjectBox(obj);
                if (!objbox)
                    return false;

                return emitObjectOp(objbox, JSOP_NEWARRAY_COPYONWRITE);
            }
        }
    }

    return emitArray(array->head(), array->count(), JSOP_NEWARRAY);
}

bool
BytecodeEmitter::emitArray(ParseNode* arrayHead, uint32_t count, JSOp op)
{

    /*
     * Emit code for [a, b, c] that is equivalent to constructing a new
     * array and in source order evaluating each element value and adding
     * it to the array, without invoking latent setters.  We use the
     * JSOP_NEWINIT and JSOP_INITELEM_ARRAY bytecodes to ignore setters and
     * to avoid dup'ing and popping the array as each element is added, as
     * JSOP_SETELEM/JSOP_SETPROP would do.
     */
    MOZ_ASSERT(op == JSOP_NEWARRAY || op == JSOP_SPREADCALLARRAY);

    uint32_t nspread = 0;
    for (ParseNode* elem = arrayHead; elem; elem = elem->pn_next) {
        if (elem->isKind(PNK_SPREAD))
            nspread++;
    }

    // Array literal's length is limited to NELEMENTS_LIMIT in parser.
    static_assert(NativeObject::MAX_DENSE_ELEMENTS_COUNT <= INT32_MAX,
                  "array literals' maximum length must not exceed limits "
                  "required by BaselineCompiler::emit_JSOP_NEWARRAY, "
                  "BaselineCompiler::emit_JSOP_INITELEM_ARRAY, "
                  "and DoSetElemFallback's handling of JSOP_INITELEM_ARRAY");
    MOZ_ASSERT(count >= nspread);
    MOZ_ASSERT(count <= NativeObject::MAX_DENSE_ELEMENTS_COUNT,
               "the parser must throw an error if the array exceeds maximum "
               "length");

    // For arrays with spread, this is a very pessimistic allocation, the
    // minimum possible final size.
    if (!emitUint32Operand(op, count - nspread))                    // ARRAY
        return false;

    ParseNode* elem = arrayHead;
    uint32_t index;
    bool afterSpread = false;
    for (index = 0; elem; index++, elem = elem->pn_next) {
        if (!afterSpread && elem->isKind(PNK_SPREAD)) {
            afterSpread = true;
            if (!emitNumberOp(index))                               // ARRAY INDEX
                return false;
        }
        if (!updateSourceCoordNotes(elem->pn_pos.begin))
            return false;

        bool allowSelfHostedIter = false;
        if (elem->isKind(PNK_ELISION)) {
            if (!emit1(JSOP_HOLE))
                return false;
        } else {
            ParseNode* expr;
            if (elem->isKind(PNK_SPREAD)) {
                expr = elem->as<UnaryNode>().kid();

                if (emitterMode == BytecodeEmitter::SelfHosting &&
                    expr->isKind(PNK_CALL) &&
                    expr->as<BinaryNode>().left()->name() == cx->names().allowContentIter)
                {
                    allowSelfHostedIter = true;
                }
            } else {
                expr = elem;
            }
            if (!emitTree(expr))                                         // ARRAY INDEX? VALUE
                return false;
        }
        if (elem->isKind(PNK_SPREAD)) {
            if (!emitIterator())                                         // ARRAY INDEX ITER
                return false;
            if (!emit2(JSOP_PICK, 2))                                    // INDEX ITER ARRAY
                return false;
            if (!emit2(JSOP_PICK, 2))                                    // ITER ARRAY INDEX
                return false;
            if (!emitSpread(allowSelfHostedIter))                        // ARRAY INDEX
                return false;
        } else if (afterSpread) {
            if (!emit1(JSOP_INITELEM_INC))
                return false;
        } else {
            if (!emitUint32Operand(JSOP_INITELEM_ARRAY, index))
                return false;
        }
    }
    MOZ_ASSERT(index == count);
    if (afterSpread) {
        if (!emit1(JSOP_POP))                                            // ARRAY
            return false;
    }
    return true;
}

bool
BytecodeEmitter::emitUnary(UnaryNode* unaryNode)
{
    if (!updateSourceCoordNotes(unaryNode->pn_pos.begin))
        return false;

    /* Unary op, including unary +/-. */
    JSOp op = unaryNode->getOp();

    if (!emitTree(unaryNode->kid()))
        return false;

    return emit1(op);
}

bool
BytecodeEmitter::emitTypeof(UnaryNode* typeofNode, JSOp op)
{
    MOZ_ASSERT(op == JSOP_TYPEOF || op == JSOP_TYPEOFEXPR);

    if (!updateSourceCoordNotes(typeofNode->pn_pos.begin))
        return false;

    if (!emitTree(typeofNode->kid()))
        return false;

    return emit1(op);
}

bool
BytecodeEmitter::emitFunctionFormalParameters(ListNode* paramsBody)
{
    ParseNode* funBody = paramsBody->last();
    FunctionBox* funbox = sc->asFunctionBox();
    bool hasRest = funbox->hasRest();

    FunctionParamsEmitter fpe(this, funbox);
    for (ParseNode* arg = paramsBody->head(); arg != funBody; arg = arg->pn_next) {
        ParseNode* bindingElement = arg;
        ParseNode* initializer = nullptr;
        if (arg->isKind(PNK_ASSIGN) || arg->isKind(PNK_INITPROP)) {
            bindingElement = arg->as<BinaryNode>().left();
            initializer = arg->as<BinaryNode>().right();
        }
        bool hasInitializer = !!initializer;
        bool isRest = hasRest && arg->pn_next == funBody;
        bool isDestructuring = !bindingElement->isKind(PNK_NAME);

        // Left-hand sides are either simple names or destructuring patterns.
        MOZ_ASSERT(bindingElement->isKind(PNK_NAME) ||
                   bindingElement->isKind(PNK_ARRAY) ||
                   bindingElement->isKind(PNK_ARRAYCOMP) ||
                   bindingElement->isKind(PNK_OBJECT));

        auto emitDefaultInitializer = [this, &initializer, &bindingElement]() {
            //            [stack]

            if (!this->emitInitializer(initializer, bindingElement)) {
                //          [stack] DEFAULT
                return false;
            }
            return true;
        };

        auto emitDestructuring = [this, &fpe, &bindingElement]() {
            //            [stack] ARG

            // If there's an parameter expression var scope, the destructuring
            // declaration needs to initialize the name in the function scope,
            // which is not the innermost scope.
            if (!this->emitDestructuringOps(&bindingElement->as<ListNode>(),
                                            fpe.getDestructuringFlavor())) {
                //          [stack] ARG
                return false;
            }

            return true;
        };

        if (isRest) {
            if (isDestructuring) {
                if (!fpe.prepareForDestructuringRest()) {
                    //        [stack]
                    return false;
                }
                if (!emitDestructuring()) {
                    //        [stack]
                    return false;
                }
                if (!fpe.emitDestructuringRestEnd()) {
                    //        [stack]
                    return false;
                }
              } else {
                  RootedAtom paramName(cx, bindingElement->as<NameNode>().name());
                  if (!fpe.emitRest(paramName)) {
                      //        [stack]
                      return false;
                  }
              }

            continue;
        }

        if (isDestructuring) {
            if (hasInitializer) {
                if (!fpe.prepareForDestructuringDefaultInitializer()) {
                    //        [stack]
                    return false;
                }
                if (!emitDefaultInitializer()) {
                    //        [stack]
                    return false;
                }
                if (!fpe.prepareForDestructuringDefault()) {
                    //        [stack]
                    return false;
                }
                if (!emitDestructuring()) {
                    //        [stack]
                    return false;
                }
                if (!fpe.emitDestructuringDefaultEnd()) {
                    //        [stack]
                    return false;
                }
              } else {
                if (!fpe.prepareForDestructuring()) {
                    //        [stack]
                    return false;
                }
                if (!emitDestructuring()) {
                    //        [stack]
                    return false;
                }
                if (!fpe.emitDestructuringEnd()) {
                    //        [stack]
                    return false;
                }
            }

            continue;
        }

        if (hasInitializer) {
            if (!fpe.prepareForDefault()) {
                //          [stack]
                return false;
            }
            if (!emitDefaultInitializer()) {
                //          [stack]
                return false;
            }
            RootedAtom paramName(cx, bindingElement->as<NameNode>().name());
            if (!fpe.emitDefaultEnd(paramName)) {
                //          [stack]
                return false;
            }

            continue;
        }

        RootedAtom paramName(cx, bindingElement->as<NameNode>().name());
        if (!fpe.emitSimple(paramName)) {
          //            [stack]
          return false;
        }
    }


    return true;
}

bool
BytecodeEmitter::emitInitializeFunctionSpecialNames()
{
    FunctionBox* funbox = sc->asFunctionBox();

    //                [stack]

    auto emitInitializeFunctionSpecialName = [](BytecodeEmitter* bce, HandlePropertyName name,
                                                JSOp op)
    {
        // A special name must be slotful, either on the frame or on the
        // call environment.
        MOZ_ASSERT(bce->lookupName(name).hasKnownSlot());

        NameOpEmitter noe(bce, name, NameOpEmitter::Kind::Initialize);
        if (!noe.prepareForRhs()) {
            //        [stack]
            return false;
        }
        if (!bce->emit1(op)) {
            //        [stack] THIS/ARGUMENTS
            return false;
        }
        if (!noe.emitAssignment())
            //        [stack] THIS/ARGUMENTS
            return false;
        if (!bce->emit1(JSOP_POP))
            //        [stack]
            return false;

        return true;
    };

    // Do nothing if the function doesn't have an arguments binding.
    if (funbox->argumentsHasLocalBinding()) {
        if (!emitInitializeFunctionSpecialName(this, cx->names().arguments, JSOP_ARGUMENTS))
            //        [stack]
            return false;
    }

    // Do nothing if the function doesn't have a this-binding (this
    // happens for instance if it doesn't use this/eval or if it's an
    // arrow function).
    if (funbox->hasThisBinding()) {
        if (!emitInitializeFunctionSpecialName(this, cx->names().dotThis, JSOP_FUNCTIONTHIS))
            return false;
    }

    return true;
}

bool
BytecodeEmitter::emitLexicalInitialization(NameNode* pn)
{
    return emitLexicalInitialization(pn->name());
}

bool
BytecodeEmitter::emitLexicalInitialization(JSAtom* name)
{
    NameOpEmitter noe(this, name, NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {
        return false;
    }

    // The caller has pushed the RHS to the top of the stack. Assert that the
    // name is lexical and no BIND[G]NAME ops were emitted.
    MOZ_ASSERT(noe.loc().isLexical());
    MOZ_ASSERT(!noe.emittedBindOp());

    if (!noe.emitAssignment()) {
        return false;
    }

    return true;
}

// This follows ES6 14.5.14 (ClassDefinitionEvaluation) and ES6 14.5.15
// (BindingClassDeclarationEvaluation).
bool
BytecodeEmitter::emitClass(ClassNode* classNode)
{
    ClassNames* names = classNode->names();
    ParseNode* heritageExpression = classNode->heritage();
    ListNode* classMembers = classNode->memberList();

    ParseNode* constructor = nullptr;
    for (ParseNode* classElement : classMembers->contents()) {
        ParseNode* unwrappedElement = classElement;
        if (unwrappedElement->is<LexicalScopeNode>())
            unwrappedElement = unwrappedElement->as<LexicalScopeNode>().scopeBody();
        if (unwrappedElement->is<ClassMethod>()) {
            ClassMethod& method = unwrappedElement->as<ClassMethod>();
            ParseNode& methodName = method.name();
            if (!method.isStatic() &&
                (methodName.isKind(PNK_OBJECT_PROPERTY_NAME) || methodName.isKind(PNK_STRING)) &&
                methodName.as<NameNode>().atom() == cx->names().constructor)
            {
                constructor = classElement;
                break;
            }
        }
    }

    //                [stack]

    ClassEmitter ce(this);
    RootedAtom innerName(cx);
    ClassEmitter::Kind kind = ClassEmitter::Kind::Expression;
    if (names) {
        innerName = names->innerBinding()->as<NameNode>().atom();
        MOZ_ASSERT(innerName);

        if (names->outerBinding()) {
            MOZ_ASSERT(names->outerBinding()->as<NameNode>().atom());
            MOZ_ASSERT(names->outerBinding()->as<NameNode>().atom() == innerName);
            kind = ClassEmitter::Kind::Declaration;
        }
    }

    if (LexicalScopeNode* scopeBindings = classNode->scopeBindings()) {
        if (!ce.emitScope(scopeBindings->scopeBindings())) {
            //            [stack]
            return false;
        }
    }

    // This is kind of silly. In order to the get the home object defined on
    // the constructor, we have to make it second, but we want the prototype
    // on top for EmitPropertyList, because we expect static properties to be
    // rarer. The result is a few more swaps than we would like. Such is life.
    bool isDerived = !!heritageExpression;
    if (isDerived) {
        if (!emitTree(heritageExpression)) {
            //          [stack] HERITAGE
            return false;
        }
        if (!ce.emitDerivedClass(innerName)) {
            //          [stack] HERITAGE HOMEOBJ
            return false;
        }
    } else {
        if (!ce.emitClass(innerName)) {
            //          [stack] HOMEOBJ
            return false;
        }
    }

    if (constructor) {
        // See |Parser::classMember(...)| for the reason why |.initializers| is
        // created within its own scope.
        Maybe<LexicalScopeEmitter> lse;
        FunctionNode* ctor;
        if (constructor->is<LexicalScopeNode>()) {
            LexicalScopeNode* constructorScope = &constructor->as<LexicalScopeNode>();

            // The constructor scope should only contain the |.initializers| binding.
            MOZ_ASSERT(!constructorScope->isEmptyScope());
            MOZ_ASSERT(constructorScope->scopeBindings()->length == 1);
            MOZ_ASSERT(constructorScope->scopeBindings()->trailingNames[0].name() ==
                       cx->names().dotInitializers);

            // As an optimization omit the |.initializers| binding when no instance
            // fields are present.
            bool hasInstanceFields = classMembers->any_of([](ParseNode* propdef) {
                return HasInitializer(propdef, false);
            });
            if (hasInstanceFields) {
                lse.emplace(this);
                if (!lse->emitScope(ScopeKind::Lexical, constructorScope->scopeBindings()))
                    return false;

                // Any class with field initializers will have a constructor
                if (!emitCreateFieldInitializers(ce, classMembers,
                                                 FieldPlacement::Instance))
                    return false;
            }
            ctor = &constructorScope->scopeBody()->as<ClassMethod>().method();
        } else {
            // The |.initializers| binding is never emitted when in self-hosting mode.
            MOZ_ASSERT(emitterMode == BytecodeEmitter::SelfHosting);
            ctor = &constructor->as<ClassMethod>().method();
        }

        bool needsHomeObject = ctor->funbox()->needsHomeObject();
        // HERITAGE is consumed inside emitFunction.
        if (!emitFunction(ctor, isDerived, classMembers)) {
            //          [stack] HOMEOBJ CTOR
            return false;
        }
        if (lse.isSome()) {
            if (!lse->emitEnd()) {
                return false;
            }
            lse.reset();
        }
        if (!ce.emitInitConstructor(needsHomeObject)) {
            //          [stack] CTOR HOMEOBJ
            return false;
        }
    } else {
        if (!ce.emitInitDefaultConstructor(Some(classNode->pn_pos.begin),
                                           Some(classNode->pn_pos.end))) {
            //          [stack] CTOR HOMEOBJ
            return false;
        }
    }

    if (!emitCreateFieldKeys(classMembers, FieldPlacement::Instance))
        return false;

    if (!emitCreateFieldInitializers(ce, classMembers, FieldPlacement::Static))
        return false;

    if (!emitCreateFieldKeys(classMembers, FieldPlacement::Static))
        return false;

    if (!emitPropertyList(classMembers, ce, ClassBody)) {
        //            [stack] CTOR HOMEOBJ
        return false;
    }

    if (!ce.emitBinding()) {
        //              [stack] CTOR
        return false;
    }

    if (!emitInitializeStaticFields(classMembers)) {
        //              [stack] CTOR
        return false;
    }

    if (!ce.emitEnd(kind)) {
        //            [stack] # class declaration
        //            [stack]
        //            [stack] # class expression
        //            [stack] CTOR
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitTree(ParseNode* pn, ValueUsage valueUsage /* = ValueUsage::WantValue */,
                          EmitLineNumberNote emitLineNote /* = EMIT_LINENOTE */)
{
    JS_CHECK_RECURSION(cx, return false);

    EmitLevelManager elm(this);

    /* Emit notes to tell the current bytecode's source line number.
       However, a couple trees require special treatment; see the
       relevant emitter functions for details. */
    if (emitLineNote == EMIT_LINENOTE && !ParseNodeRequiresSpecialLineNumberNotes(pn)) {
        if (!updateLineNumberNotes(pn->pn_pos.begin))
            return false;
    }

    switch (pn->getKind()) {
      case PNK_FUNCTION:
        if (!emitFunction(&pn->as<FunctionNode>()))
            return false;
        break;

      case PNK_PARAMSBODY:
        MOZ_ASSERT_UNREACHABLE("ParamsBody should be handled in emitFunctionScript.");
        break;

      case PNK_IF:
        if (!emitIf(&pn->as<TernaryNode>()))
            return false;
        break;

      case PNK_SWITCH:
        if (!emitSwitch(&pn->as<SwitchStatement>()))
            return false;
        break;

      case PNK_WHILE:
        if (!emitWhile(&pn->as<BinaryNode>()))
            return false;
        break;

      case PNK_DOWHILE:
        if (!emitDo(&pn->as<BinaryNode>()))
            return false;
        break;

      case PNK_FOR:
        if (!emitFor(&pn->as<ForNode>()))
            return false;
        break;

      case PNK_COMPREHENSIONFOR:
        if (!emitComprehensionFor(&pn->as<ForNode>()))
            return false;
        break;

      case PNK_BREAK:
        if (!emitBreak(pn->as<BreakStatement>().label()))
            return false;
        break;

      case PNK_CONTINUE:
        if (!emitContinue(pn->as<ContinueStatement>().label()))
            return false;
        break;

      case PNK_WITH:
        if (!emitWith(&pn->as<BinaryNode>()))
            return false;
        break;

      case PNK_TRY:
        if (!emitTry(&pn->as<TryNode>()))
            return false;
        break;

      case PNK_CATCH:
        if (!emitCatch(&pn->as<TernaryNode>()))
            return false;
        break;

      case PNK_VAR:
        if (!emitDeclarationList(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_RETURN:
        if (!emitReturn(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_YIELD_STAR:
        if (!emitYieldStar(pn->as<UnaryNode>().kid()))
            return false;
        break;

      case PNK_GENERATOR:
        if (!emit1(JSOP_GENERATOR))
            return false;
        break;

      case PNK_INITIALYIELD:
        if (!emitInitialYield(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_YIELD:
        if (!emitYield(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_AWAIT:
        if (!emitAwaitInInnermostScope(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_STATEMENTLIST:
        if (!emitStatementList(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_SEMI:
        if (!emitStatement(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_LABEL:
        if (!emitLabeledStatement(&pn->as<LabeledStatement>()))
            return false;
        break;

      case PNK_COMMA:
        if (!emitSequenceExpr(&pn->as<ListNode>(), valueUsage))
            return false;
        break;

      case PNK_INITPROP:
      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
      case PNK_SUBASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_MULASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
      case PNK_POWASSIGN: {
        BinaryNode* assignNode = &pn->as<BinaryNode>();
        if (!emitAssignmentOrInit(assignNode->getKind(), assignNode->getOp(),
                                  assignNode->left(), assignNode->right()))
            return false;
        break;
      }

      case PNK_COALESCEASSIGN:
      case PNK_ORASSIGN:
      case PNK_ANDASSIGN: {
        BinaryNode* assignNode = &pn->as<BinaryNode>();
        if (!emitShortCircuitAssignment(assignNode->getKind(), assignNode->getOp(),
                                        assignNode->left(), assignNode->right())) {
          return false;
        }
        break;
      }

      case PNK_CONDITIONAL:
        if (!emitConditionalExpression(pn->as<ConditionalExpression>(), valueUsage))
            return false;
        break;

      case PNK_COALESCE:
      case PNK_OR:
      case PNK_AND:
        if (!emitLogical(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_ADD:
      case PNK_SUB:
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_IN:
      case PNK_INSTANCEOF:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
        if (!emitLeftAssociative(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_POW:
        if (!emitRightAssociative(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_TYPEOFNAME:
        if (!emitTypeof(&pn->as<UnaryNode>(), JSOP_TYPEOF))
            return false;
        break;

      case PNK_TYPEOFEXPR:
        if (!emitTypeof(&pn->as<UnaryNode>(), JSOP_TYPEOFEXPR))
            return false;
        break;

      case PNK_THROW:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
        if (!emitUnary(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_PREINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTINCREMENT:
      case PNK_POSTDECREMENT:
        if (!emitIncOrDec(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_DELETENAME:
        if (!emitDeleteName(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_DELETEPROP:
        if (!emitDeleteProperty(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_DELETEELEM:
        if (!emitDeleteElement(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_DELETEEXPR:
        if (!emitDeleteExpression(&pn->as<UnaryNode>()))
            return false;
        break;

      case PNK_DELETEOPTCHAIN:
        if (!emitDeleteOptionalChain(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case PNK_OPTCHAIN:
        if (!emitOptionalChain(&pn->as<UnaryNode>(), valueUsage)) {
            return false;
        }
        break;

      case PNK_DOT: {
        PropertyAccess* prop = &pn->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        PropOpEmitter poe(this,
                          PropOpEmitter::Kind::Get,
                          isSuper
                          ? PropOpEmitter::ObjKind::Super
                          : PropOpEmitter::ObjKind::Other);
        if (!poe.prepareForObj()) {
            return false;
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {          // THIS
                return false;
            }
        } else {
            if (!emitPropLHS(prop)) {                      // OBJ
                return false;
            }
        }
        if (!poe.emitGet(prop->key().atom())) {            // PROP
            return false;
        }
        break;
      }

      case PNK_ELEM: {
        PropertyByValue* elem = &pn->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter eoe(this,
                          ElemOpEmitter::Kind::Get,
                          isSuper
                          ? ElemOpEmitter::ObjKind::Super
                          : ElemOpEmitter::ObjKind::Other);
        if (!emitElemObjAndKey(elem, isSuper, eoe)) {     // [Super]
            //                                            // THIS KEY
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        if (!eoe.emitGet()) {                             // ELEM
            return false;
        }
        break;
      }

      case PNK_NEW:
      case PNK_TAGGED_TEMPLATE:
      case PNK_CALL:
      case PNK_GENEXP:
      case PNK_SUPERCALL:
        if (!emitCallOrNew(&pn->as<BinaryNode>(), valueUsage))
            return false;
        break;

      case PNK_LEXICALSCOPE:
        if (!emitLexicalScope(&pn->as<LexicalScopeNode>()))
            return false;
        break;

      case PNK_CONST:
      case PNK_LET:
        if (!emitDeclarationList(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_IMPORT:
        MOZ_ASSERT(sc->isModuleContext());
        break;

      case PNK_EXPORT: {
        MOZ_ASSERT(sc->isModuleContext());
        UnaryNode* node = &pn->as<UnaryNode>();
        ParseNode* decl = node->kid();
        if (decl->getKind() != PNK_EXPORT_SPEC_LIST) {
            if (!emitTree(decl))
                return false;
        }
        break;
      }

      case PNK_EXPORT_DEFAULT: {
        MOZ_ASSERT(sc->isModuleContext());
        BinaryNode* ed = &pn->as<BinaryNode>();
        if (!emitTree(ed->left()))
            return false;
        if (ed->right()) {
            if (!emitLexicalInitialization(&ed->right()->as<NameNode>()))
                return false;
            if (!emit1(JSOP_POP))
                return false;
        }
        break;
      }

      case PNK_EXPORT_FROM:
        MOZ_ASSERT(sc->isModuleContext());
        break;

      case PNK_ARRAYPUSH:
        /*
         * The array object's stack index is in arrayCompDepth. See below
         * under the array initialiser code generator for array comprehension
         * special casing.
         */
        if (!emitTree(pn->as<UnaryNode>().kid()))
            return false;
        if (!emitDupAt(this->stackDepth - 1 - arrayCompDepth))
            return false;
        if (!emit1(JSOP_ARRAYPUSH))
            return false;
        break;

      case PNK_CALLSITEOBJ:
        if (!emitCallSiteObject(&pn->as<CallSiteNode>()))
            return false;
        break;

      case PNK_ARRAY:
        if (!emitArrayLiteral(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_ARRAYCOMP:
        if (!emitArrayComp(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_OBJECT:
        if (!emitObject(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_NAME:
        if (!emitGetName(pn))
            return false;
        break;

      case PNK_TEMPLATE_STRING_LIST:
        if (!emitTemplateString(&pn->as<ListNode>()))
            return false;
        break;

      case PNK_TEMPLATE_STRING:
      case PNK_STRING:
        if (!emitAtomOp(pn->as<NameNode>().atom(), JSOP_STRING))
            return false;
        break;

      case PNK_NUMBER:
        if (!emitNumberOp(pn->as<NumericLiteral>().value()))
            return false;
        break;

      case PNK_BIGINT:
        if (!emitBigIntOp(pn->as<BigIntLiteral>().box()->value())) {
            return false;
        }
        break;

      case PNK_REGEXP:
        if (!emitRegExp(objectList.add(pn->as<RegExpLiteral>().objbox())))
            return false;
        break;

      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_NULL:
      case PNK_RAW_UNDEFINED:
        if (!emit1(pn->getOp()))
            return false;
        break;

      case PNK_THIS:
        if (!emitThisLiteral(&pn->as<ThisLiteral>()))
            return false;
        break;

      case PNK_DEBUGGER:
        if (!updateSourceCoordNotes(pn->pn_pos.begin))
            return false;
        if (!emit1(JSOP_DEBUGGER))
            return false;
        break;

      case PNK_NOP:
        MOZ_ASSERT(pn->getArity() == PN_NULLARY);
        break;

      case PNK_CLASS:
        if (!emitClass(&pn->as<ClassNode>()))
            return false;
        break;

      case PNK_NEWTARGET:
        if (!emit1(JSOP_NEWTARGET))
            return false;
        break;

      case PNK_IMPORT_META:
        if (!emit1(JSOP_IMPORTMETA))
            return false;
        break;

      case PNK_CALL_IMPORT:
        if (!cx->compartment()->runtimeFromAnyThread()->moduleDynamicImportHook) {
            reportError(nullptr, JSMSG_NO_DYNAMIC_IMPORT);
            return false;
        }
        if (!emitTree(pn->as<BinaryNode>().right()) || !emit1(JSOP_DYNAMIC_IMPORT)) {
            return false;
        }
        break;

      case PNK_SETTHIS:
        if (!emitSetThis(&pn->as<BinaryNode>()))
            return false;
        break;

      case PNK_PROPERTYNAME:
      case PNK_POSHOLDER:
        MOZ_FALLTHROUGH_ASSERT("Should never try to emit PNK_POSHOLDER or PNK_PROPERTYNAME");

      default:
        MOZ_ASSERT(0);
    }

    /* bce->emitLevel == 1 means we're last on the stack, so finish up. */
    if (emitLevel == 1) {
        if (!updateSourceCoordNotes(pn->pn_pos.end))
            return false;
    }
    return true;
}

bool
BytecodeEmitter::emitTreeInBranch(ParseNode* pn,
                                  ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    // Code that may be conditionally executed always need their own TDZ
    // cache.
    TDZCheckCache tdzCache(this);
    return emitTree(pn, valueUsage);
}

/*
 * Special `emitTree` for Optional Chaining case.
 * Examples of this are `emitOptionalChain`, and `emitDeleteOptionalChain`.
 */
bool
BytecodeEmitter::emitOptionalTree(
    ParseNode* pn,
    OptionalEmitter& oe,
    ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    JS_CHECK_RECURSION(cx, return false);

    ParseNodeKind kind = pn->getKind();
    switch (kind) {
      case PNK_OPTDOT: {
        OptionalPropertyAccess* prop = &pn->as<OptionalPropertyAccess>();
        bool isSuper = false;
        PropOpEmitter poe(this, PropOpEmitter::Kind::Get,
                          PropOpEmitter::ObjKind::Other);
        if (!emitOptionalDotExpression(prop, poe, isSuper, oe))
            return false;
        break;
      }
      case PNK_DOT: {
        PropertyAccess* prop = &pn->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        PropOpEmitter poe(this, PropOpEmitter::Kind::Get,
                          isSuper ? PropOpEmitter::ObjKind::Super
                                  : PropOpEmitter::ObjKind::Other);
        if (!emitOptionalDotExpression(prop, poe, isSuper, oe))
            return false;
        break;
      }
      case PNK_OPTELEM: {
        OptionalPropertyByValue* elem = &pn->as<OptionalPropertyByValue>();
        bool isSuper = false;
        ElemOpEmitter eoe(this, ElemOpEmitter::Kind::Get,
                          ElemOpEmitter::ObjKind::Other);
        if (!emitOptionalElemExpression(elem, eoe, isSuper, oe))
            return false;
        break;
      }
      case PNK_ELEM: {
        PropertyByValue* elem = &pn->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter eoe(this, ElemOpEmitter::Kind::Get,
                          isSuper ? ElemOpEmitter::ObjKind::Super
                                  : ElemOpEmitter::ObjKind::Other);
        if (!emitOptionalElemExpression(elem, eoe, isSuper, oe))
            return false;
        break;
      }
      case PNK_CALL:
      case PNK_OPTCALL: {
        if (!emitOptionalCall(&pn->as<BinaryNode>(), oe, valueUsage)) {
            return false;
        }
        break;
      }
      // List of accepted ParseNodeKinds that might appear only at the beginning
      // of an Optional Chain.
      // For example, a taggedTemplateExpr node might occur if we have
      // `test`?.b, with `test` as the taggedTemplateExpr ParseNode.
      default: {
#ifdef DEBUG
        // https://tc39.es/ecma262/#sec-primary-expression
        bool isPrimaryExpression =
            kind == PNK_THIS ||
            kind == PNK_NAME ||
            kind == PNK_NULL ||
            kind == PNK_TRUE ||
            kind == PNK_FALSE ||
            kind == PNK_NUMBER ||
            kind == PNK_STRING ||
            kind == PNK_ARRAY ||
            kind == PNK_OBJECT ||
            kind == PNK_FUNCTION ||
            kind == PNK_CLASS ||
            kind == PNK_REGEXP ||
            kind == PNK_TEMPLATE_STRING ||
            kind == PNK_RAW_UNDEFINED ||
            pn->isInParens();

        // https://tc39.es/ecma262/#sec-left-hand-side-expressions
        bool isMemberExpression = isPrimaryExpression ||
                                  kind == PNK_TAGGED_TEMPLATE ||
                                  kind == PNK_NEW ||
                                  kind == PNK_NEWTARGET;
                                  //kind == ParseNodeKind::ImportMetaExpr;

        bool isCallExpression = kind == PNK_SETTHIS;
                                //kind == ParseNodeKind::CallImportExpr;

        MOZ_ASSERT(isMemberExpression || isCallExpression,
                   "Unknown ParseNodeKind for OptionalChain");
#endif
        return emitTree(pn);
      }
    }

    return true;
}

// Handle the case of a call made on a OptionalChainParseNode.
// For example `(a?.b)()` and `(a?.b)?.()`.
bool
BytecodeEmitter::emitCalleeAndThisForOptionalChain(
    UnaryNode* optionalChain,
    ParseNode* callNode,
    CallOrNewEmitter& cone)
{
    ParseNode* calleeNode = optionalChain->kid();

    // Create a new OptionalEmitter, in order to emit the right bytecode
    // in isolation.
    OptionalEmitter oe(this, stackDepth);

    if (!emitOptionalCalleeAndThis(callNode, calleeNode, cone, oe)) {
        //              [stack] CALLEE THIS
        return false;
    }

    // Complete the jump if necessary. This will set both the "this" value
    // and the "callee" value to undefined, if the callee is undefined. It
    // does not matter much what the this value is, the function call will
    // fail if it is not optional, and be set to undefined otherwise.
    if (!oe.emitOptionalJumpTarget(JSOP_UNDEFINED,
                                   OptionalEmitter::Kind::Reference)) {
        //              [stack] # If shortcircuit
        //              [stack] UNDEFINED UNDEFINED
        //              [stack] # otherwise
        //              [stack] CALLEE THIS
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitOptionalChain(
    UnaryNode* optionalChain,
    ValueUsage valueUsage)
{
    ParseNode* expression = optionalChain->kid();

    OptionalEmitter oe(this, stackDepth);

    if (!emitOptionalTree(expression, oe, valueUsage)) {
        //              [stack] VAL
        return false;
    }

    if (!oe.emitOptionalJumpTarget(JSOP_UNDEFINED)) {
        //              [stack] # If shortcircuit
        //              [stack] UNDEFINED
        //              [stack] # otherwise
        //              [stack] VAL
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitOptionalDotExpression(
    PropertyAccessBase* prop,
    PropOpEmitter& poe,
    bool isSuper,
    OptionalEmitter& oe)
{
    if (!poe.prepareForObj()) {
        //              [stack]
        return false;
    }

    if (isSuper) {
        if (!emitGetThisForSuperBase(&prop->expression().as<UnaryNode>())) {
            //            [stack] OBJ
            return false;
        }
    } else {
        if (!emitOptionalTree(&prop->expression(), oe)) {
            //            [stack] OBJ
            return false;
        }
    }

    if (prop->isKind(PNK_OPTDOT)) {
        MOZ_ASSERT(!isSuper);
        if (!oe.emitJumpShortCircuit()) {
            //            [stack] # if Jump
            //            [stack] UNDEFINED-OR-NULL
            //            [stack] # otherwise
            //            [stack] OBJ
            return false;
        }
    }

    if (!poe.emitGet(prop->key().atom())) {
        //              [stack] PROP
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitOptionalElemExpression(
    PropertyByValueBase* elem,
    ElemOpEmitter& eoe,
    bool isSuper,
    OptionalEmitter& oe)
{
    if (!eoe.prepareForObj()) {
        //              [stack]
        return false;
    }

    if (isSuper) {
        if (!emitGetThisForSuperBase(&elem->expression().as<UnaryNode>())) {
            //            [stack] OBJ
            return false;
        }
    } else {
        if (!emitOptionalTree(&elem->expression(), oe)) {
            //              [stack] OBJ
            return false;
        }
    }

    if (elem->isKind(PNK_OPTELEM)) {
        MOZ_ASSERT(!isSuper);
        if (!oe.emitJumpShortCircuit()) {
            //            [stack] # if Jump
            //            [stack] UNDEFINED-OR-NULL
            //            [stack] # otherwise
            //            [stack] OBJ
            return false;
        }
    }

    if (!eoe.prepareForKey()) {
        //              [stack] OBJ? OBJ
        return false;
    }

    if (!emitTree(&elem->key())) {
        //              [stack] OBJ? OBJ KEY
        return false;
    }

    if (!eoe.emitGet()) {
        //              [stack] ELEM
        return false;
    }

    return true;
}

static bool
AllocSrcNote(ExclusiveContext* cx, SrcNotesVector& notes, unsigned* index)
{
    size_t oldLength = notes.length();

    if (MOZ_UNLIKELY(oldLength + 1 > MaxSrcNotesLength)) {
        ReportAllocationOverflow(cx);
        return false;
    }

    if (!notes.growBy(1)) {
        ReportOutOfMemory(cx);
        return false;
    }

    *index = oldLength;
    return true;
}

bool
BytecodeEmitter::newSrcNote(SrcNoteType type, unsigned* indexp)
{
    SrcNotesVector& notes = this->notes();
    unsigned index;
    if (!AllocSrcNote(cx, notes, &index))
        return false;

    /*
     * Compute delta from the last annotated bytecode's offset.  If it's too
     * big to fit in sn, allocate one or more xdelta notes and reset sn.
     */
    ptrdiff_t offset = this->offset();
    ptrdiff_t delta = offset - lastNoteOffset();
    current->lastNoteOffset = offset;
    if (delta >= SN_DELTA_LIMIT) {
        do {
            ptrdiff_t xdelta = Min(delta, SN_XDELTA_MASK);
            SN_MAKE_XDELTA(&notes[index], xdelta);
            delta -= xdelta;
            if (!AllocSrcNote(cx, notes, &index))
                return false;
        } while (delta >= SN_DELTA_LIMIT);
    }

    /*
     * Initialize type and delta, then allocate the minimum number of notes
     * needed for type's arity.  Usually, we won't need more, but if an offset
     * does take two bytes, setSrcNoteOffset will grow notes.
     */
    SN_MAKE_NOTE(&notes[index], type, delta);
    for (int n = (int)js_SrcNoteSpec[type].arity; n > 0; n--) {
        if (!newSrcNote(SRC_NULL))
            return false;
    }

    if (indexp)
        *indexp = index;
    return true;
}

bool
BytecodeEmitter::newSrcNote2(SrcNoteType type, ptrdiff_t offset, unsigned* indexp)
{
    unsigned index;
    if (!newSrcNote(type, &index))
        return false;
    if (!setSrcNoteOffset(index, 0, offset))
        return false;
    if (indexp)
        *indexp = index;
    return true;
}

bool
BytecodeEmitter::newSrcNote3(SrcNoteType type, ptrdiff_t offset1, ptrdiff_t offset2,
                             unsigned* indexp)
{
    unsigned index;
    if (!newSrcNote(type, &index))
        return false;
    if (!setSrcNoteOffset(index, 0, offset1))
        return false;
    if (!setSrcNoteOffset(index, 1, offset2))
        return false;
    if (indexp)
        *indexp = index;
    return true;
}

bool
BytecodeEmitter::addToSrcNoteDelta(jssrcnote* sn, ptrdiff_t delta)
{
    /*
     * Called only from finishTakingSrcNotes to add to main script note
     * deltas, and only by a small positive amount.
     */
    MOZ_ASSERT(current == &main);
    MOZ_ASSERT((unsigned) delta < (unsigned) SN_XDELTA_LIMIT);

    ptrdiff_t base = SN_DELTA(sn);
    ptrdiff_t limit = SN_IS_XDELTA(sn) ? SN_XDELTA_LIMIT : SN_DELTA_LIMIT;
    ptrdiff_t newdelta = base + delta;
    if (newdelta < limit) {
        SN_SET_DELTA(sn, newdelta);
    } else {
        jssrcnote xdelta;
        SN_MAKE_XDELTA(&xdelta, delta);
        if (!main.notes.insert(sn, xdelta))
            return false;
    }
    return true;
}

bool
BytecodeEmitter::setSrcNoteOffset(unsigned index, unsigned which, ptrdiff_t offset)
{
    if (!SN_REPRESENTABLE_OFFSET(offset)) {
        parser->tokenStream.reportError(JSMSG_NEED_DIET, js_script_str);
        return false;
    }

    SrcNotesVector& notes = this->notes();

    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    jssrcnote* sn = &notes[index];
    MOZ_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    MOZ_ASSERT((int) which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_4BYTE_OFFSET_FLAG)
            sn += 3;
    }

    /*
     * See if the new offset requires four bytes either by being too big or if
     * the offset has already been inflated (in which case, we need to stay big
     * to not break the srcnote encoding if this isn't the last srcnote).
     */
    if (offset > (ptrdiff_t)SN_4BYTE_OFFSET_MASK || (*sn & SN_4BYTE_OFFSET_FLAG)) {
        /* Maybe this offset was already set to a four-byte value. */
        if (!(*sn & SN_4BYTE_OFFSET_FLAG)) {
            /* Insert three dummy bytes that will be overwritten shortly. */
            if (MOZ_UNLIKELY(notes.length() + 3 > MaxSrcNotesLength)) {
                ReportAllocationOverflow(cx);
                return false;
            }
            jssrcnote dummy = 0;
            if (!(sn = notes.insert(sn, dummy)) ||
                !(sn = notes.insert(sn, dummy)) ||
                !(sn = notes.insert(sn, dummy)))
            {
                ReportOutOfMemory(cx);
                return false;
            }
        }
        *sn++ = (jssrcnote)(SN_4BYTE_OFFSET_FLAG | (offset >> 24));
        *sn++ = (jssrcnote)(offset >> 16);
        *sn++ = (jssrcnote)(offset >> 8);
    }
    *sn = (jssrcnote)offset;
    return true;
}

bool
BytecodeEmitter::finishTakingSrcNotes(uint32_t* out)
{
    MOZ_ASSERT(current == &main);

    unsigned prologueCount = prologue.notes.length();
    if (prologueCount && prologue.currentLine != firstLine) {
        switchToPrologue();
        if (!newSrcNote2(SRC_SETLINE, ptrdiff_t(firstLine)))
            return false;
        switchToMain();
    } else {
        /*
         * Either no prologue srcnotes, or no line number change over prologue.
         * We don't need a SRC_SETLINE, but we may need to adjust the offset
         * of the first main note, by adding to its delta and possibly even
         * prepending SRC_XDELTA notes to it to account for prologue bytecodes
         * that came at and after the last annotated bytecode.
         */
        ptrdiff_t offset = prologueOffset() - prologue.lastNoteOffset;
        MOZ_ASSERT(offset >= 0);
        if (offset > 0 && main.notes.length() != 0) {
            /* NB: Use as much of the first main note's delta as we can. */
            jssrcnote* sn = main.notes.begin();
            ptrdiff_t delta = SN_IS_XDELTA(sn)
                            ? SN_XDELTA_MASK - (*sn & SN_XDELTA_MASK)
                            : SN_DELTA_MASK - (*sn & SN_DELTA_MASK);
            if (offset < delta)
                delta = offset;
            for (;;) {
                if (!addToSrcNoteDelta(sn, delta))
                    return false;
                offset -= delta;
                if (offset == 0)
                    break;
                delta = Min(offset, SN_XDELTA_MASK);
                sn = main.notes.begin();
            }
        }
    }

    // The prologue count might have changed, so we can't reuse prologueCount.
    // The + 1 is to account for the final SN_MAKE_TERMINATOR that is appended
    // when the notes are copied to their final destination by CopySrcNotes.
    *out = prologue.notes.length() + main.notes.length() + 1;
    return true;
}

void
BytecodeEmitter::copySrcNotes(jssrcnote* destination, uint32_t nsrcnotes)
{
    unsigned prologueCount = prologue.notes.length();
    unsigned mainCount = main.notes.length();
    unsigned totalCount = prologueCount + mainCount;
    MOZ_ASSERT(totalCount == nsrcnotes - 1);
    if (prologueCount)
        PodCopy(destination, prologue.notes.begin(), prologueCount);
    PodCopy(destination + prologueCount, main.notes.begin(), mainCount);
    SN_MAKE_TERMINATOR(&destination[totalCount]);
}

OptionalEmitter::OptionalEmitter(BytecodeEmitter* bce, int32_t initialDepth)
  : bce_(bce),
    tdzCache_(bce),
    initialDepth_(initialDepth)
{
}

bool
OptionalEmitter::emitJumpShortCircuit() {
    MOZ_ASSERT(state_ == State::Start ||
               state_ == State::ShortCircuit ||
               state_ == State::ShortCircuitForCall);
    MOZ_ASSERT(initialDepth_ + 1 == bce_->stackDepth);

    InternalIfEmitter ifEmitter(bce_);
    if (!bce_->emitPushNotUndefinedOrNull()) {
        //              [stack] OBJ NOT-UNDEFINED-OR-NULL
        return false;
    }

    if (!bce_->emit1(JSOP_NOT)) {
        //              [stack] OBJ UNDEFINED-OR-NULL
        return false;
    }

    if (!ifEmitter.emitThen()) {
        return false;
    }

    if (!bce_->newSrcNote(SRC_OPTCHAIN)) {
        return false;
    }

    if (!bce_->emitJump(JSOP_GOTO, &jumpShortCircuit_)) {
        //              [stack] UNDEFINED-OR-NULL
        return false;
    }

    if (!ifEmitter.emitEnd()) {
        return false;
    }
#ifdef DEBUG
    state_ = State::ShortCircuit;
#endif
    return true;
}

bool
OptionalEmitter::emitJumpShortCircuitForCall() {
    MOZ_ASSERT(state_ == State::Start ||
               state_ == State::ShortCircuit ||
               state_ == State::ShortCircuitForCall);
    int32_t depth = bce_->stackDepth;
    MOZ_ASSERT(initialDepth_ + 2 == depth);
    if (!bce_->emit1(JSOP_SWAP)) {
        //              [stack] THIS CALLEE
        return false;
    }

    InternalIfEmitter ifEmitter(bce_);
    if (!bce_->emitPushNotUndefinedOrNull()) {
        //              [stack] THIS CALLEE NOT-UNDEFINED-OR-NULL
        return false;
    }

    if (!bce_->emit1(JSOP_NOT)) {
        //              [stack] THIS CALLEE UNDEFINED-OR-NULL
        return false;
    }

    if (!ifEmitter.emitThen()) {
        return false;
    }

    if (!bce_->emit1(JSOP_POP)) {
        //              [stack] VAL
        return false;
    }

    if (!bce_->newSrcNote(SRC_OPTCHAIN)) {
        return false;
    }

    if (!bce_->emitJump(JSOP_GOTO, &jumpShortCircuit_)) {
        //              [stack] UNDEFINED-OR-NULL
        return false;
    }

    if (!ifEmitter.emitEnd()) {
        return false;
    }

    bce_->stackDepth = depth;

    if (!bce_->emit1(JSOP_SWAP)) {
        //              [stack] THIS CALLEE
        return false;
    }
#ifdef DEBUG
    state_ = State::ShortCircuitForCall;
#endif
    return true;
}

bool
OptionalEmitter::emitOptionalJumpTarget(JSOp op,
                                        Kind kind /* = Kind::Other */) {
#ifdef DEBUG
    int32_t depth = bce_->stackDepth;
#endif
    MOZ_ASSERT(state_ == State::ShortCircuit ||
               state_ == State::ShortCircuitForCall);

    // if we get to this point, it means that the optional chain did not short
    // circuit, so we should skip the short circuiting bytecode.
    if (!bce_->emitJump(JSOP_GOTO, &jumpFinish_)) {
        //              [stack] # if call
        //              [stack] CALLEE THIS
        //              [stack] # otherwise, if defined
        //              [stack] VAL
        //              [stack] # otherwise
        //              [stack] UNDEFINED-OR-NULL
        return false;
    }

    if (!bce_->emitJumpTargetAndPatch(jumpShortCircuit_)) {
        //              [stack] UNDEFINED-OR-NULL
        return false;
    }

    // reset stack depth to the depth when we jumped
    bce_->stackDepth = initialDepth_ + 1;

    if (!bce_->emit1(JSOP_POP)) {
        //              [stack]
        return false;
    }

    if (!bce_->emit1(op)) {
        //              [stack] JSOP
        return false;
    }

    if (kind == Kind::Reference) {
        if (!bce_->emit1(op)) {
            //              [stack] JSOP JSOP
            return false;
        }
    }

    MOZ_ASSERT(depth == bce_->stackDepth);

    if (!bce_->emitJumpTargetAndPatch(jumpFinish_)) {
        //              [stack] # if call
        //              [stack] CALLEE THIS
        //              [stack] # otherwise
        //              [stack] VAL
        return false;
    }
#ifdef DEBUG
    state_ = State::JumpEnd;
#endif
    return true;
}

void
CGConstList::finish(ConstArray* array)
{
    MOZ_ASSERT(length() == array->length);

    for (unsigned i = 0; i < length(); i++)
        array->vector[i] = vector[i];
}

/*
 * Find the index of the given object for code generator.
 *
 * Since the emitter refers to each parsed object only once, for the index we
 * use the number of already indexed objects. We also add the object to a list
 * to convert the list to a fixed-size array when we complete code generation,
 * see js::CGObjectList::finish below.
 */
unsigned
CGObjectList::add(ObjectBox* objbox)
{
    MOZ_ASSERT(objbox->isObjectBox());
    MOZ_ASSERT(!objbox->emitLink);
    objbox->emitLink = lastbox;
    lastbox = objbox;
    return length++;
}

unsigned
CGObjectList::indexOf(JSObject* obj)
{
    MOZ_ASSERT(length > 0);
    unsigned index = length - 1;
    for (ObjectBox* box = lastbox; box->object() != obj; box = box->emitLink)
        index--;
    return index;
}

void
CGObjectList::finish(ObjectArray* array)
{
    MOZ_ASSERT(length <= INDEX_LIMIT);
    MOZ_ASSERT(length == array->length);

    js::GCPtrObject* cursor = array->vector + array->length;
    ObjectBox* objbox = lastbox;
    do {
        --cursor;
        MOZ_ASSERT(!*cursor);
        MOZ_ASSERT(objbox->object()->isTenured());
        *cursor = objbox->object();
    } while ((objbox = objbox->emitLink) != nullptr);
    MOZ_ASSERT(cursor == array->vector);
}

ObjectBox*
CGObjectList::find(uint32_t index)
{
    MOZ_ASSERT(index < length);
    ObjectBox* box = lastbox;
    for (unsigned n = length - 1; n > index; n--)
        box = box->emitLink;
    return box;
}

void
CGScopeList::finish(ScopeArray* array)
{
    MOZ_ASSERT(length() <= INDEX_LIMIT);
    MOZ_ASSERT(length() == array->length);
    for (uint32_t i = 0; i < length(); i++)
        array->vector[i].init(vector[i]);
}

bool
CGTryNoteList::append(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end)
{
    MOZ_ASSERT(start <= end);
    MOZ_ASSERT(size_t(uint32_t(start)) == start);
    MOZ_ASSERT(size_t(uint32_t(end)) == end);

    JSTryNote note;
    note.kind = kind;
    note.stackDepth = stackDepth;
    note.start = uint32_t(start);
    note.length = uint32_t(end - start);

    return list.append(note);
}

void
CGTryNoteList::finish(TryNoteArray* array)
{
    MOZ_ASSERT(length() == array->length);

    for (unsigned i = 0; i < length(); i++)
        array->vector[i] = list[i];
}

bool
CGScopeNoteList::append(uint32_t scopeIndex, uint32_t offset, bool inPrologue,
                        uint32_t parent)
{
    CGScopeNote note;
    mozilla::PodZero(&note);

    note.index = scopeIndex;
    note.start = offset;
    note.parent = parent;
    note.startInPrologue = inPrologue;

    return list.append(note);
}

void
CGScopeNoteList::recordEnd(uint32_t index, uint32_t offset, bool inPrologue)
{
    MOZ_ASSERT(index < length());
    MOZ_ASSERT(list[index].length == 0);
    list[index].end = offset;
    list[index].endInPrologue = inPrologue;
}

void
CGScopeNoteList::finish(ScopeNoteArray* array, uint32_t prologueLength)
{
    MOZ_ASSERT(length() == array->length);

    for (unsigned i = 0; i < length(); i++) {
        if (!list[i].startInPrologue)
            list[i].start += prologueLength;
        if (!list[i].endInPrologue && list[i].end != UINT32_MAX)
            list[i].end += prologueLength;
        MOZ_ASSERT(list[i].end >= list[i].start);
        list[i].length = list[i].end - list[i].start;
        array->vector[i] = list[i];
    }
}

void
CGYieldAndAwaitOffsetList::finish(YieldAndAwaitOffsetArray& array, uint32_t prologueLength)
{
    MOZ_ASSERT(length() == array.length());

    for (unsigned i = 0; i < length(); i++)
        array[i] = prologueLength + list[i];
}

/*
 * We should try to get rid of offsetBias (always 0 or 1, where 1 is
 * JSOP_{NOP,POP}_LENGTH), which is used only by SRC_FOR.
 */
const JSSrcNoteSpec js_SrcNoteSpec[] = {
#define DEFINE_SRC_NOTE_SPEC(sym, name, arity) { name, arity },
    FOR_EACH_SRC_NOTE_TYPE(DEFINE_SRC_NOTE_SPEC)
#undef DEFINE_SRC_NOTE_SPEC
};

static int
SrcNoteArity(jssrcnote* sn)
{
    MOZ_ASSERT(SN_TYPE(sn) < SRC_LAST);
    return js_SrcNoteSpec[SN_TYPE(sn)].arity;
}

JS_FRIEND_API(unsigned)
js::SrcNoteLength(jssrcnote* sn)
{
    unsigned arity;
    jssrcnote* base;

    arity = SrcNoteArity(sn);
    for (base = sn++; arity; sn++, arity--) {
        if (*sn & SN_4BYTE_OFFSET_FLAG)
            sn += 3;
    }
    return sn - base;
}

JS_FRIEND_API(ptrdiff_t)
js::GetSrcNoteOffset(jssrcnote* sn, unsigned which)
{
    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    MOZ_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    MOZ_ASSERT((int) which < SrcNoteArity(sn));
    for (sn++; which; sn++, which--) {
        if (*sn & SN_4BYTE_OFFSET_FLAG)
            sn += 3;
    }
    if (*sn & SN_4BYTE_OFFSET_FLAG) {
        return (ptrdiff_t)(((uint32_t)(sn[0] & SN_4BYTE_OFFSET_MASK) << 24)
                           | (sn[1] << 16)
                           | (sn[2] << 8)
                           | sn[3]);
    }
    return (ptrdiff_t)*sn;
}
