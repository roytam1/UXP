/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/mips64/Assembler-mips64.h"

#include "mozilla/DebugOnly.h"

using mozilla::DebugOnly;

using namespace js;
using namespace js::jit;

ABIArgGenerator::ABIArgGenerator()
  : usedArgSlots_(0),
    firstArgFloat(false),
    current_()
{}

ABIArg
ABIArgGenerator::next(MIRType type)
{
    switch (type) {
      case MIRType::Int32:
      case MIRType::Int64:
      case MIRType::Pointer: {
        Register destReg;
        if (GetIntArgReg(usedArgSlots_, &destReg))
            current_ = ABIArg(destReg);
        else
            current_ = ABIArg(GetArgStackDisp(usedArgSlots_));
        usedArgSlots_++;
        break;
      }
      case MIRType::Float32:
      case MIRType::Double: {
        FloatRegister destFReg;
        FloatRegister::ContentType contentType;
        if (!usedArgSlots_)
            firstArgFloat = true;
        contentType = (type == MIRType::Double) ?
            FloatRegisters::Double : FloatRegisters::Single;
        if (GetFloatArgReg(usedArgSlots_, &destFReg))
            current_ = ABIArg(FloatRegister(destFReg.id(), contentType));
        else
            current_ = ABIArg(GetArgStackDisp(usedArgSlots_));
        usedArgSlots_++;
        break;
      }
      default:
        MOZ_CRASH("Unexpected argument type");
    }
    return current_;
}

uint32_t
js::jit::RT(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::TotalPhys);
    return r.id() << RTShift;
}

uint32_t
js::jit::RD(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::TotalPhys);
    return r.id() << RDShift;
}

uint32_t
js::jit::RZ(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::TotalPhys);
    return r.id() << RZShift;
}

uint32_t
js::jit::SA(FloatRegister r)
{
    MOZ_ASSERT(r.id() < FloatRegisters::TotalPhys);
    return r.id() << SAShift;
}

// Used to patch jumps created by MacroAssemblerMIPS64Compat::jumpWithPatch.
void
jit::PatchJump(CodeLocationJump& jump_, CodeLocationLabel label, ReprotectCode reprotect)
{
    Instruction* inst;

    inst = AssemblerMIPSShared::GetInstructionImmediateFromJump((Instruction*)jump_.raw());

    // Six instructions used in load 64-bit imm.
    MaybeAutoWritableJitCode awjc(inst, 6 * sizeof(uint32_t), reprotect);
    Assembler::UpdateLoad64Value(inst, (uint64_t)label.raw());

    AutoFlushICache::flush(uintptr_t(inst), 6 * sizeof(uint32_t));
}

// For more infromation about backedges look at comment in
// MacroAssemblerMIPS64Compat::backedgeJump()
void
jit::PatchBackedge(CodeLocationJump& jump, CodeLocationLabel label,
                   JitRuntime::BackedgeTarget target)
{
    uintptr_t sourceAddr = (uintptr_t)jump.raw();
    uintptr_t targetAddr = (uintptr_t)label.raw();
    InstImm* branch = (InstImm*)jump.raw();

    MOZ_ASSERT(branch->extractOpcode() == (uint32_t(op_beq) >> OpcodeShift));

    if (BOffImm16::IsInRange(targetAddr - sourceAddr)) {
        branch->setBOffImm16(BOffImm16(targetAddr - sourceAddr));
    } else {
        if (target == JitRuntime::BackedgeLoopHeader) {
            Instruction* inst = &branch[1];
            Assembler::UpdateLoad64Value(inst, targetAddr);
            // Jump to first ori. The lui will be executed in delay slot.
            branch->setBOffImm16(BOffImm16(2 * sizeof(uint32_t)));
        } else {
            Instruction* inst = &branch[6];
            Assembler::UpdateLoad64Value(inst, targetAddr);
            // Jump to first ori of interrupt loop.
            branch->setBOffImm16(BOffImm16(6 * sizeof(uint32_t)));
        }
    }
}

uintptr_t
Assembler::GetPointer(uint8_t* instPtr)
{
    Instruction* inst = (Instruction*)instPtr;
    return Assembler::ExtractLoad64Value(inst);
}

static JitCode *
CodeFromJump(Instruction* jump)
{
    uint8_t* target = (uint8_t*)Assembler::ExtractLoad64Value(jump);
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
    void* ptr = (void*)Assembler::ExtractLoad64Value(inst);
    void* prior = ptr;

    // All pointers on MIPS64 will have the top bits cleared. If those bits
    // are not cleared, this must be a Value.
    uintptr_t word = reinterpret_cast<uintptr_t>(ptr);
    if (word >> JSVAL_TAG_SHIFT) {
        Value v = Value::fromRawBits(word);
        TraceManuallyBarrieredEdge(trc, &v, "ion-masm-value");
        ptr = (void*)v.bitsAsPunboxPointer();
    } else {
        // No barrier needed since these are constants.
        TraceManuallyBarrieredGenericPointerEdge(trc, reinterpret_cast<gc::Cell**>(&ptr),
                                                     "ion-masm-ptr");
    }

    if (ptr != prior) {
        Assembler::UpdateLoad64Value(inst, uint64_t(ptr));
        AutoFlushICache::flush(uintptr_t(inst), 6 * sizeof(uint32_t));
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
        Assembler::UpdateLoad64Value(inst, (uint64_t)address);
    }
}

uint64_t
Assembler::ExtractLoad64Value(Instruction* inst0)
{
    InstImm* i0 = (InstImm*) inst0;
    InstImm* i1 = (InstImm*) i0->next();
    InstReg* i2 = (InstReg*) i1->next();
    InstImm* i3 = (InstImm*) i2->next();
    InstImm* i5 = (InstImm*) i3->next()->next();

    MOZ_ASSERT(i0->extractOpcode() == ((uint32_t)op_lui >> OpcodeShift));
    MOZ_ASSERT(i1->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));
    MOZ_ASSERT(i3->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));

    if ((i2->extractOpcode() == ((uint32_t)op_special >> OpcodeShift)) &&
        (i2->extractFunctionField() == ff_dsrl32))
    {
        uint64_t value = (uint64_t(i0->extractImm16Value()) << 32) |
                         (uint64_t(i1->extractImm16Value()) << 16) |
                         uint64_t(i3->extractImm16Value());
        return uint64_t((int64_t(value) <<16) >> 16);
    }

    MOZ_ASSERT(i5->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));
    uint64_t value = (uint64_t(i0->extractImm16Value()) << 48) |
                     (uint64_t(i1->extractImm16Value()) << 32) |
                     (uint64_t(i3->extractImm16Value()) << 16) |
                     uint64_t(i5->extractImm16Value());
    return value;
}

void
Assembler::UpdateLoad64Value(Instruction* inst0, uint64_t value)
{
    InstImm* i0 = (InstImm*) inst0;
    InstImm* i1 = (InstImm*) i0->next();
    InstReg* i2 = (InstReg*) i1->next();
    InstImm* i3 = (InstImm*) i2->next();
    InstImm* i5 = (InstImm*) i3->next()->next();

    MOZ_ASSERT(i0->extractOpcode() == ((uint32_t)op_lui >> OpcodeShift));
    MOZ_ASSERT(i1->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));
    MOZ_ASSERT(i3->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));

    if ((i2->extractOpcode() == ((uint32_t)op_special >> OpcodeShift)) &&
        (i2->extractFunctionField() == ff_dsrl32))
    {
        i0->setImm16(Imm16::Lower(Imm32(value >> 32)));
        i1->setImm16(Imm16::Upper(Imm32(value)));
        i3->setImm16(Imm16::Lower(Imm32(value)));
        return;
    }

    MOZ_ASSERT(i5->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));

    i0->setImm16(Imm16::Upper(Imm32(value >> 32)));
    i1->setImm16(Imm16::Lower(Imm32(value >> 32)));
    i3->setImm16(Imm16::Upper(Imm32(value)));
    i5->setImm16(Imm16::Lower(Imm32(value)));
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
    DebugOnly<uint64_t> value = Assembler::ExtractLoad64Value(inst);
    MOZ_ASSERT(value == uint64_t(expectedValue.value));

    // Replace with new value
    Assembler::UpdateLoad64Value(inst, uint64_t(newValue.value));

    AutoFlushICache::flush(uintptr_t(inst), 6 * sizeof(uint32_t));
}

void
Assembler::PatchInstructionImmediate(uint8_t* code, PatchedImmPtr imm)
{
    Assembler::UpdateLoad64Value((Instruction*)code, (uint64_t)imm.value);
}

uint64_t
Assembler::ExtractInstructionImmediate(uint8_t* code)
{
    InstImm* inst = (InstImm*)code;
    return Assembler::ExtractLoad64Value(inst);
}

void
Assembler::ToggleCall(CodeLocationLabel inst_, bool enabled)
{
    Instruction* inst = (Instruction*)inst_.raw();
    InstImm* i0 = (InstImm*) inst;
    InstImm* i1 = (InstImm*) i0->next();
    InstImm* i3 = (InstImm*) i1->next()->next();
    Instruction* i4 = (Instruction*) i3->next();

    MOZ_ASSERT(i0->extractOpcode() == ((uint32_t)op_lui >> OpcodeShift));
    MOZ_ASSERT(i1->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));
    MOZ_ASSERT(i3->extractOpcode() == ((uint32_t)op_ori >> OpcodeShift));

    if (enabled) {
        MOZ_ASSERT(i4->extractOpcode() != ((uint32_t)op_lui >> OpcodeShift));
        InstReg jalr = InstReg(op_special, ScratchRegister, zero, ra, ff_jalr);
        *i4 = jalr;
    } else {
        InstNOP nop;
        *i4 = nop;
    }

    AutoFlushICache::flush(uintptr_t(i4), sizeof(uint32_t));
}
