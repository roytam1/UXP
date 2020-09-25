/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/mips32/Assembler-mips32.h"

#include "mozilla/DebugOnly.h"

using mozilla::DebugOnly;

using namespace js;
using namespace js::jit;

ABIArgGenerator::ABIArgGenerator()
  : usedArgSlots_(0),
    firstArgFloatSize_(0),
    useGPRForFloats_(false),
    current_()
{}

ABIArg
ABIArgGenerator::next(MIRType type)
{
    Register destReg;
    switch (type) {
      case MIRType::Int32:
      case MIRType::Pointer:
        if (GetIntArgReg(usedArgSlots_, &destReg))
            current_ = ABIArg(destReg);
        else
            current_ = ABIArg(usedArgSlots_ * sizeof(intptr_t));
        usedArgSlots_++;
        break;
      case MIRType::Int64:
        if (!usedArgSlots_) {
            current_ = ABIArg(a0, a1);
            usedArgSlots_ = 2;
        } else if (usedArgSlots_ <= 2) {
            current_ = ABIArg(a2, a3);
            usedArgSlots_ = 4;
        } else {
            if (usedArgSlots_ < NumIntArgRegs)
                usedArgSlots_ = NumIntArgRegs;
            usedArgSlots_ += usedArgSlots_ % 2;
            current_ = ABIArg(usedArgSlots_ * sizeof(intptr_t));
            usedArgSlots_ += 2;
        }
        break;
      case MIRType::Float32:
        if (!usedArgSlots_) {
            current_ = ABIArg(f12.asSingle());
            firstArgFloatSize_ = 1;
        } else if (usedArgSlots_ == firstArgFloatSize_) {
            current_ = ABIArg(f14.asSingle());
        } else if (useGPRForFloats_ && GetIntArgReg(usedArgSlots_, &destReg)) {
            current_ = ABIArg(destReg);
        } else {
            if (usedArgSlots_ < NumIntArgRegs)
                usedArgSlots_ = NumIntArgRegs;
            current_ = ABIArg(usedArgSlots_ * sizeof(intptr_t));
        }
        usedArgSlots_++;
        break;
      case MIRType::Double:
        if (!usedArgSlots_) {
            current_ = ABIArg(f12);
            usedArgSlots_ = 2;
            firstArgFloatSize_ = 2;
        } else if (usedArgSlots_ == firstArgFloatSize_) {
            current_ = ABIArg(f14);
            usedArgSlots_ = 4;
        } else if (useGPRForFloats_ && usedArgSlots_ <= 2) {
            current_ = ABIArg(a2, a3);
            usedArgSlots_ = 4;
        } else {
            if (usedArgSlots_ < NumIntArgRegs)
                usedArgSlots_ = NumIntArgRegs;
            usedArgSlots_ += usedArgSlots_ % 2;
            current_ = ABIArg(usedArgSlots_ * sizeof(intptr_t));
            usedArgSlots_ += 2;
        }
        break;
      default:
        MOZ_CRASH("Unexpected argument type");
    }
    return current_;
}

uint32_t
js::jit::RT(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::RegisterIdLimit);
    return r.id() << RTShift;
}

uint32_t
js::jit::RD(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::RegisterIdLimit);
    return r.id() << RDShift;
}

uint32_t
js::jit::RZ(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::RegisterIdLimit);
    return r.id() << RZShift;
}

uint32_t
js::jit::SA(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::RegisterIdLimit);
    return r.id() << SAShift;
}

// Used to patch jumps created by MacroAssemblerMIPSCompat::jumpWithPatch.
void
jit::PatchJump(CodeLocationJump& jump_, CodeLocationLabel label, ReprotectCode reprotect)
{
    Instruction* inst1;
    Instruction* inst2;

    inst1 = AssemblerMIPSShared::GetInstructionImmediateFromJump((Instruction*)jump_.raw());
    inst2 = inst1->next();

    MaybeAutoWritableJitCode awjc(inst1, 8, reprotect);
    AssemblerMIPSShared::UpdateLuiOriValue(inst1, inst2, (uint32_t)label.raw());

    AutoFlushICache::flush(uintptr_t(inst1), 8);
}

// For more infromation about backedges look at comment in
// MacroAssemblerMIPSCompat::backedgeJump()
void
jit::PatchBackedge(CodeLocationJump& jump, CodeLocationLabel label,
                   JitRuntime::BackedgeTarget target)
{
    uint32_t sourceAddr = (uint32_t)jump.raw();
    uint32_t targetAddr = (uint32_t)label.raw();
    InstImm* branch = (InstImm*)jump.raw();

    MOZ_ASSERT(branch->extractOpcode() == (uint32_t(op_beq) >> OpcodeShift));

    if (BOffImm16::IsInRange(targetAddr - sourceAddr)) {
        branch->setBOffImm16(BOffImm16(targetAddr - sourceAddr));
    } else {
        if (target == JitRuntime::BackedgeLoopHeader) {
            Instruction* lui = &branch[1];
            AssemblerMIPSShared::UpdateLuiOriValue(lui, lui->next(), targetAddr);
            // Jump to ori. The lui will be executed in delay slot.
            branch->setBOffImm16(BOffImm16(2 * sizeof(uint32_t)));
        } else {
            Instruction* lui = &branch[4];
            AssemblerMIPSShared::UpdateLuiOriValue(lui, lui->next(), targetAddr);
            branch->setBOffImm16(BOffImm16(4 * sizeof(uint32_t)));
        }
    }
}

uintptr_t
Assembler::GetPointer(uint8_t* instPtr)
{
    Instruction* inst = (Instruction*)instPtr;
    return Assembler::ExtractLuiOriValue(inst, inst->next());
}

static JitCode*
CodeFromJump(Instruction* jump)
{
    uint8_t* target = (uint8_t*)Assembler::ExtractLuiOriValue(jump, jump->next());
    return JitCode::FromExecutable(target);
}

void
Assembler::TraceJumpRelocations(JSTracer* trc, JitCode* code, CompactBufferReader& reader)
{
    while (reader.more()) {
        JitCode* child = CodeFromJump((Instruction*)(code->raw() + reader.readUnsigned()));
        TraceManuallyBarrieredEdge(trc, &child, "rel32");
    }
}

static void
TraceOneDataRelocation(JSTracer* trc, Instruction* inst)
{
    void* ptr = (void*)Assembler::ExtractLuiOriValue(inst, inst->next());
    void* prior = ptr;

    // No barrier needed since these are constants.
    TraceManuallyBarrieredGenericPointerEdge(trc, reinterpret_cast<gc::Cell**>(&ptr),
                                                 "ion-masm-ptr");
    if (ptr != prior) {
        AssemblerMIPSShared::UpdateLuiOriValue(inst, inst->next(), uint32_t(ptr));
        AutoFlushICache::flush(uintptr_t(inst), 8);
    }
}

static void
TraceDataRelocations(JSTracer* trc, uint8_t* buffer, CompactBufferReader& reader)
{
    while (reader.more()) {
        size_t offset = reader.readUnsigned();
        Instruction* inst = (Instruction*)(buffer + offset);
        TraceOneDataRelocation(trc, inst);
    }
}

static void
TraceDataRelocations(JSTracer* trc, MIPSBuffer* buffer, CompactBufferReader& reader)
{
    while (reader.more()) {
        BufferOffset bo (reader.readUnsigned());
        MIPSBuffer::AssemblerBufferInstIterator iter(bo, buffer);
        TraceOneDataRelocation(trc, iter.cur());
    }
}

void
Assembler::TraceDataRelocations(JSTracer* trc, JitCode* code, CompactBufferReader& reader)
{
    ::TraceDataRelocations(trc, code->raw(), reader);
}

Assembler::Condition
Assembler::UnsignedCondition(Condition cond)
{
    switch (cond) {
      case Zero:
      case NonZero:
        return cond;
      case LessThan:
      case Below:
        return Below;
      case LessThanOrEqual:
      case BelowOrEqual:
        return BelowOrEqual;
      case GreaterThan:
      case Above:
        return Above;
      case AboveOrEqual:
      case GreaterThanOrEqual:
        return AboveOrEqual;
      default:
        MOZ_CRASH("unexpected condition");
    }
}

Assembler::Condition
Assembler::ConditionWithoutEqual(Condition cond)
{
    switch (cond) {
      case LessThan:
      case LessThanOrEqual:
        return LessThan;
      case Below:
      case BelowOrEqual:
        return Below;
      case GreaterThan:
      case GreaterThanOrEqual:
        return GreaterThan;
      case Above:
      case AboveOrEqual:
        return Above;
      default:
        MOZ_CRASH("unexpected condition");
    }
}

void
Assembler::trace(JSTracer* trc)
{
    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch& rp = jumps_[i];
        if (rp.kind == Relocation::JITCODE) {
            JitCode* code = JitCode::FromExecutable((uint8_t*)rp.target);
            TraceManuallyBarrieredEdge(trc, &code, "masmrel32");
            MOZ_ASSERT(code == JitCode::FromExecutable((uint8_t*)rp.target));
        }
    }
    if (dataRelocations_.length()) {
        CompactBufferReader reader(dataRelocations_);
        ::TraceDataRelocations(trc, &m_buffer, reader);
    }
}

void
Assembler::Bind(uint8_t* rawCode, CodeOffset* label, const void* address)
{
    if (label->bound()) {
        intptr_t offset = label->offset();
        Instruction* inst = (Instruction*) (rawCode + offset);
        AssemblerMIPSShared::UpdateLuiOriValue(inst, inst->next(), (uint32_t)address);
    }
}

uint32_t
Assembler::ExtractLuiOriValue(Instruction* inst0, Instruction* inst1)
{
    InstImm* i0 = (InstImm*) inst0;
    InstImm* i1 = (InstImm*) inst1;
    MOZ_ASSERT(i0->extractOpcode() == ((uint32_t)op_lui >> OpcodeShift));
    MOZ_ASSERT(i1->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));

    uint32_t value = i0->extractImm16Value() << 16;
    value = value | i1->extractImm16Value();
    return value;
}

void
Assembler::PatchDataWithValueCheck(CodeLocationLabel label, ImmPtr newValue,
                                   ImmPtr expectedValue)
{
    PatchDataWithValueCheck(label, PatchedImmPtr(newValue.value),
                            PatchedImmPtr(expectedValue.value));
}

void
Assembler::PatchDataWithValueCheck(CodeLocationLabel label, PatchedImmPtr newValue,
                                   PatchedImmPtr expectedValue)
{
    Instruction* inst = (Instruction*) label.raw();

    // Extract old Value
    DebugOnly<uint32_t> value = Assembler::ExtractLuiOriValue(&inst[0], &inst[1]);
    MOZ_ASSERT(value == uint32_t(expectedValue.value));

    // Replace with new value
    AssemblerMIPSShared::UpdateLuiOriValue(inst, inst->next(), uint32_t(newValue.value));

    AutoFlushICache::flush(uintptr_t(inst), 8);
}

void
Assembler::PatchInstructionImmediate(uint8_t* code, PatchedImmPtr imm)
{
    InstImm* inst = (InstImm*)code;
    AssemblerMIPSShared::UpdateLuiOriValue(inst, inst->next(), (uint32_t)imm.value);
}

uint32_t
Assembler::ExtractInstructionImmediate(uint8_t* code)
{
    InstImm* inst = (InstImm*)code;
    return Assembler::ExtractLuiOriValue(inst, inst->next());
}

void
Assembler::ToggleCall(CodeLocationLabel inst_, bool enabled)
{
    Instruction* inst = (Instruction*)inst_.raw();
    InstImm* i0 = (InstImm*) inst;
    InstImm* i1 = (InstImm*) i0->next();
    Instruction* i2 = (Instruction*) i1->next();

    MOZ_ASSERT(i0->extractOpcode() == ((uint32_t)op_lui >> OpcodeShift));
    MOZ_ASSERT(i1->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));

    if (enabled) {
        InstReg jalr = InstReg(op_special, ScratchRegister, zero, ra, ff_jalr);
        *i2 = jalr;
    } else {
        InstNOP nop;
        *i2 = nop;
    }

    AutoFlushICache::flush(uintptr_t(i2), 4);
}
