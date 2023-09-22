/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2015 Mozilla Foundation
 * Copyright 2023 Moonchild Productions
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

#include "wasm/WasmBinaryIterator.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

#ifdef DEBUG
OpKind
wasm::Classify(Op op)
{
    switch (op) {
      case Op::Block:
        return OpKind::Block;
      case Op::Loop:
        return OpKind::Loop;
      case Op::Unreachable:
        return OpKind::Unreachable;
      case Op::Drop:
        return OpKind::Drop;
      case Op::I32Const:
        return OpKind::I32;
      case Op::I64Const:
        return OpKind::I64;
      case Op::F32Const:
        return OpKind::F32;
      case Op::F64Const:
        return OpKind::F64;
      case Op::Br:
        return OpKind::Br;
      case Op::BrIf:
        return OpKind::BrIf;
      case Op::BrTable:
        return OpKind::BrTable;
      case Op::Nop:
        return OpKind::Nop;
      case Op::I32Clz:
      case Op::I32Ctz:
      case Op::I32Popcnt:
      case Op::I64Clz:
      case Op::I64Ctz:
      case Op::I64Popcnt:
      case Op::F32Abs:
      case Op::F32Neg:
      case Op::F32Ceil:
      case Op::F32Floor:
      case Op::F32Trunc:
      case Op::F32Nearest:
      case Op::F32Sqrt:
      case Op::F64Abs:
      case Op::F64Neg:
      case Op::F64Ceil:
      case Op::F64Floor:
      case Op::F64Trunc:
      case Op::F64Nearest:
      case Op::F64Sqrt:
      case Op::I32BitNot:
      case Op::I32Abs:
      case Op::F64Sin:
      case Op::F64Cos:
      case Op::F64Tan:
      case Op::F64Asin:
      case Op::F64Acos:
      case Op::F64Atan:
      case Op::F64Exp:
      case Op::F64Log:
      case Op::I32Neg:
        return OpKind::Unary;
      case Op::I32Add:
      case Op::I32Sub:
      case Op::I32Mul:
      case Op::I32DivS:
      case Op::I32DivU:
      case Op::I32RemS:
      case Op::I32RemU:
      case Op::I32And:
      case Op::I32Or:
      case Op::I32Xor:
      case Op::I32Shl:
      case Op::I32ShrS:
      case Op::I32ShrU:
      case Op::I32Rotl:
      case Op::I32Rotr:
      case Op::I64Add:
      case Op::I64Sub:
      case Op::I64Mul:
      case Op::I64DivS:
      case Op::I64DivU:
      case Op::I64RemS:
      case Op::I64RemU:
      case Op::I64And:
      case Op::I64Or:
      case Op::I64Xor:
      case Op::I64Shl:
      case Op::I64ShrS:
      case Op::I64ShrU:
      case Op::I64Rotl:
      case Op::I64Rotr:
      case Op::F32Add:
      case Op::F32Sub:
      case Op::F32Mul:
      case Op::F32Div:
      case Op::F32Min:
      case Op::F32Max:
      case Op::F32CopySign:
      case Op::F64Add:
      case Op::F64Sub:
      case Op::F64Mul:
      case Op::F64Div:
      case Op::F64Min:
      case Op::F64Max:
      case Op::F64CopySign:
      case Op::I32Min:
      case Op::I32Max:
      case Op::F64Mod:
      case Op::F64Pow:
      case Op::F64Atan2:
        return OpKind::Binary;
      case Op::I32Eq:
      case Op::I32Ne:
      case Op::I32LtS:
      case Op::I32LtU:
      case Op::I32LeS:
      case Op::I32LeU:
      case Op::I32GtS:
      case Op::I32GtU:
      case Op::I32GeS:
      case Op::I32GeU:
      case Op::I64Eq:
      case Op::I64Ne:
      case Op::I64LtS:
      case Op::I64LtU:
      case Op::I64LeS:
      case Op::I64LeU:
      case Op::I64GtS:
      case Op::I64GtU:
      case Op::I64GeS:
      case Op::I64GeU:
      case Op::F32Eq:
      case Op::F32Ne:
      case Op::F32Lt:
      case Op::F32Le:
      case Op::F32Gt:
      case Op::F32Ge:
      case Op::F64Eq:
      case Op::F64Ne:
      case Op::F64Lt:
      case Op::F64Le:
      case Op::F64Gt:
      case Op::F64Ge:
        return OpKind::Comparison;
      case Op::I32Eqz:
      case Op::I32WrapI64:
      case Op::I32TruncSF32:
      case Op::I32TruncUF32:
      case Op::I32ReinterpretF32:
      case Op::I32TruncSF64:
      case Op::I32TruncUF64:
      case Op::I64ExtendSI32:
      case Op::I64ExtendUI32:
      case Op::I64TruncSF32:
      case Op::I64TruncUF32:
      case Op::I64TruncSF64:
      case Op::I64TruncUF64:
      case Op::I64ReinterpretF64:
      case Op::I64Eqz:
      case Op::F32ConvertSI32:
      case Op::F32ConvertUI32:
      case Op::F32ReinterpretI32:
      case Op::F32ConvertSI64:
      case Op::F32ConvertUI64:
      case Op::F32DemoteF64:
      case Op::F64ConvertSI32:
      case Op::F64ConvertUI32:
      case Op::F64ConvertSI64:
      case Op::F64ConvertUI64:
      case Op::F64ReinterpretI64:
      case Op::F64PromoteF32:
        return OpKind::Conversion;
      case Op::I32Load8S:
      case Op::I32Load8U:
      case Op::I32Load16S:
      case Op::I32Load16U:
      case Op::I64Load8S:
      case Op::I64Load8U:
      case Op::I64Load16S:
      case Op::I64Load16U:
      case Op::I64Load32S:
      case Op::I64Load32U:
      case Op::I32Load:
      case Op::I64Load:
      case Op::F32Load:
      case Op::F64Load:
        return OpKind::Load;
      case Op::I32Store8:
      case Op::I32Store16:
      case Op::I64Store8:
      case Op::I64Store16:
      case Op::I64Store32:
      case Op::I32Store:
      case Op::I64Store:
      case Op::F32Store:
      case Op::F64Store:
        return OpKind::Store;
      case Op::I32TeeStore8:
      case Op::I32TeeStore16:
      case Op::I64TeeStore8:
      case Op::I64TeeStore16:
      case Op::I64TeeStore32:
      case Op::I32TeeStore:
      case Op::I64TeeStore:
      case Op::F32TeeStore:
      case Op::F64TeeStore:
      case Op::F32TeeStoreF64:
      case Op::F64TeeStoreF32:
        return OpKind::TeeStore;
      case Op::Select:
        return OpKind::Select;
      case Op::GetLocal:
        return OpKind::GetLocal;
      case Op::SetLocal:
        return OpKind::SetLocal;
      case Op::TeeLocal:
        return OpKind::TeeLocal;
      case Op::GetGlobal:
        return OpKind::GetGlobal;
      case Op::SetGlobal:
        return OpKind::SetGlobal;
      case Op::TeeGlobal:
        return OpKind::TeeGlobal;
      case Op::Call:
        return OpKind::Call;
      case Op::CallIndirect:
        return OpKind::CallIndirect;
      case Op::OldCallIndirect:
        return OpKind::OldCallIndirect;
      case Op::Return:
      case Op::Limit:
        // Accept Limit, for use in decoding the end of a function after the body.
        return OpKind::Return;
      case Op::If:
        return OpKind::If;
      case Op::Else:
        return OpKind::Else;
      case Op::End:
        return OpKind::End;
      case Op::I32AtomicsLoad:
        return OpKind::AtomicLoad;
      case Op::I32AtomicsStore:
        return OpKind::AtomicStore;
      case Op::I32AtomicsBinOp:
        return OpKind::AtomicBinOp;
      case Op::I32AtomicsCompareExchange:
        return OpKind::AtomicCompareExchange;
      case Op::I32AtomicsExchange:
        return OpKind::AtomicExchange;
      case Op::CurrentMemory:
        return OpKind::CurrentMemory;
      case Op::GrowMemory:
        return OpKind::GrowMemory;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unimplemented opcode");
}
#endif
