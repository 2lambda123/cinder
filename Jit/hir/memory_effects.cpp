// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "Jit/hir/memory_effects.h"

#include "Jit/hir/hir.h"

namespace jit {
namespace hir {

namespace {
// Instructions that don't produce a borrowed reference or steal any of their
// inputs.
MemoryEffects commonEffects(const Instr& inst, AliasClass may_store) {
  return {false, AEmpty, {inst.NumOperands()}, may_store};
}

// Instructions that borrow their output from a specific location.
MemoryEffects borrowFrom(const Instr& inst, AliasClass borrow_support) {
  return {true, borrow_support, {inst.NumOperands()}, AEmpty};
}

util::BitVector stealAllInputs(const Instr& inst) {
  return {inst.NumOperands(), (uint64_t{1} << inst.NumOperands()) - 1};
}

} // namespace

MemoryEffects memoryEffects(const Instr& inst) {
  switch (inst.opcode()) {
    // Instructions that don't produce a borrowed reference, don't steal any
    // inputs, and don't write to heap locations that we track.
    case Opcode::kAssign:
    case Opcode::kBuildSlice:
    case Opcode::kBuildString:
    case Opcode::kDeopt:
    case Opcode::kDeoptPatchpoint:
    case Opcode::kDoubleBinaryOp:
    case Opcode::kFormatValue:
    case Opcode::kGuardType:
    case Opcode::kHintType:
    case Opcode::kIntBinaryOp:
    case Opcode::kIsSubtype:
    case Opcode::kPrimitiveUnaryOp:
    case Opcode::kPrimitiveBox:
    case Opcode::kPrimitiveCompare:
    case Opcode::kIntConvert:
    case Opcode::kPrimitiveUnbox:
    case Opcode::kIsNegativeAndErrOccurred:
    case Opcode::kIsErrStopAsyncIteration:
    case Opcode::kLoadEvalBreaker:
    case Opcode::kLoadVarObjectSize:
    case Opcode::kLongCompare:
    case Opcode::kMakeCell:
    case Opcode::kMakeCheckedDict:
    case Opcode::kMakeCheckedList:
    case Opcode::kMakeDict:
    case Opcode::kMakeFunction:
    case Opcode::kMakeListTuple:
    case Opcode::kMakeSet:
    case Opcode::kMakeTupleFromList:
    case Opcode::kRefineType:
    case Opcode::kSnapshot:
    case Opcode::kTpAlloc:
    case Opcode::kUnicodeCompare:
    case Opcode::kUseType:
    case Opcode::kWaitHandleLoadCoroOrResult:
    case Opcode::kWaitHandleLoadWaiter:
      return commonEffects(inst, AEmpty);

    // These push/pop shadow frames and should not get DCE'd.
    case Opcode::kBeginInlinedFunction:
    case Opcode::kEndInlinedFunction:
      return commonEffects(inst, AAny);

    // Can write to fields of its operands.
    case Opcode::kWaitHandleRelease:
    case Opcode::kSetCurrentAwaiter:
      return commonEffects(inst, AOther);

    // These can deopt but don't write to any memory locations when they fall
    // through.
    case Opcode::kCheckExc:
    case Opcode::kCheckField:
    case Opcode::kCheckFreevar:
    case Opcode::kCheckNeg:
    case Opcode::kCheckSequenceBounds:
    case Opcode::kCheckVar:
    case Opcode::kGuard:
      return commonEffects(inst, AEmpty);

    // Instructions that don't produce a borrowed reference, don't steal any
    // inputs, and may write all memory locations (usually from invoking
    // arbitrary user code).
    case Opcode::kBinaryOp:
    case Opcode::kCallEx:
    case Opcode::kCallExKw:
    case Opcode::kCallMethod:
    case Opcode::kCallStatic:
    case Opcode::kCallStaticRetVoid:
    case Opcode::kClearError:
    case Opcode::kCompare:
    case Opcode::kDeleteAttr:
    case Opcode::kDeleteSubscr:
    case Opcode::kCompareBool:
    case Opcode::kFillTypeAttrCache:
    case Opcode::kGetIter:
    case Opcode::kInPlaceOp:
    case Opcode::kInvokeIterNext:
    case Opcode::kInvokeStaticFunction:
    case Opcode::kInvokeMethod:
    case Opcode::kIsInstance:
    case Opcode::kIsTruthy:
    case Opcode::kLoadAttr:
    case Opcode::kLoadAttrSpecial:
    case Opcode::kLoadAttrSuper:
    case Opcode::kLoadGlobal:
    case Opcode::kLoadMethod:
    case Opcode::kLoadMethodSuper:
    case Opcode::kLongBinaryOp:
    case Opcode::kRepeatList:
    case Opcode::kRepeatTuple:
    case Opcode::kUnaryOp:
    case Opcode::kImportFrom:
    case Opcode::kImportName:
    case Opcode::kUnpackExToTuple:
    case Opcode::kVectorCall:
    case Opcode::kVectorCallKW:
    case Opcode::kVectorCallStatic:
      return commonEffects(inst, AManagedHeapAny);

    // Steals the reference to its second input and gives it to the cell
    case Opcode::kSetCellItem:
      return {true, AEmpty, {inst.NumOperands(), 2}, ACellItem};

    // Returns a stolen (from the cell), not borrowed, reference.
    case Opcode::kStealCellItem:
      return commonEffects(inst, AEmpty);

    // Instructions that return nullptr or a borrowed reference to a singleton
    // (usually None or True), and can invoke user code.
    case Opcode::kRunPeriodicTasks:
    case Opcode::kMergeDictUnpack:
    case Opcode::kMergeSetUnpack:
    case Opcode::kSetDictItem:
    case Opcode::kSetSetItem:
    case Opcode::kStoreAttr:
    case Opcode::kStoreSubscr:
      return {true, AEmpty, {}, AManagedHeapAny};

    case Opcode::kListAppend:
    case Opcode::kListExtend:
      return {true, AEmpty, {inst.NumOperands()}, AListItem};

    case Opcode::kIncref:
    case Opcode::kXIncref:
      return {false, AEmpty, {inst.NumOperands()}, AOther};

    case Opcode::kBatchDecref:
    case Opcode::kDecref:
    case Opcode::kXDecref:
      return {false, AEmpty, {1, 1}, AManagedHeapAny};

    case Opcode::kInitFunction:
      // InitFunction mostly writes to a bunch of func fields we don't track,
      // but it can also invoke the JIT which may at some point have effects
      // worth tracking.
      return commonEffects(inst, AOther);

    case Opcode::kInitListTuple: {
      // Steal all inputs except the first, which is the container to
      // initialize.
      util::BitVector inputs{inst.NumOperands()};
      inputs.fill(true);
      inputs.SetBit(0, false);
      auto may_store = static_cast<const InitListTuple&>(inst).is_tuple()
          ? ATupleItem
          : AListItem;
      return {false, AEmpty, std::move(inputs), may_store};
    }

    case Opcode::kStoreField:
      JIT_DCHECK(inst.NumOperands() == 3, "Unexpected number of operands");
      return {false, AEmpty, {3, 2}, AInObjectAttr};

    case Opcode::kLoadArg:
    case Opcode::kLoadCurrentFunc:
      return borrowFrom(inst, AFuncArgs);

    case Opcode::kLoadConst:
    case Opcode::kGuardIs:
      return borrowFrom(inst, AEmpty);

    case Opcode::kLoadCellItem:
      return borrowFrom(inst, ACellItem);

    case Opcode::kLoadField: {
      auto& ldfld = static_cast<const LoadField&>(inst);
      if (ldfld.borrowed()) {
        return borrowFrom(inst, AInObjectAttr);
      }
      return commonEffects(inst, AEmpty);
    }

    case Opcode::kLoadFieldAddress:
      return commonEffects(inst, AEmpty);

    case Opcode::kLoadFunctionIndirect:

    case Opcode::kLoadGlobalCached:
      return borrowFrom(inst, AGlobal);

    case Opcode::kLoadTupleItem:
      return borrowFrom(inst, ATupleItem);

    case Opcode::kLoadArrayItem:
      return borrowFrom(inst, AArrayItem | AListItem);
    case Opcode::kStoreArrayItem:
      // we steal a ref to our third operand, the value being stored
      return {
          false, AEmpty, {inst.NumOperands(), 1 << 2}, AArrayItem | AListItem};
    case Opcode::kLoadTypeAttrCacheItem:
      return borrowFrom(inst, ATypeAttrCache);

    case Opcode::kReturn:
      return {false, AEmpty, {1, 1}, AManagedHeapAny};

    case Opcode::kSetFunctionAttr: {
      JIT_DCHECK(inst.NumOperands() == 2, "Unexpected number of operands");
      return {false, AEmpty, {2, 1}, AFuncAttr};
    }

    case Opcode::kRaise:
      return {false, AEmpty, stealAllInputs(inst), AEmpty};

    case Opcode::kRaiseAwaitableError:
    case Opcode::kRaiseStatic:
      return commonEffects(inst, AManagedHeapAny);

    // The outputs of InitialYield and YieldValue are the `arg` argument to
    // _PyJIT_GenSend(), which is borrowed from its caller like all arguments
    // to C functions.
    case Opcode::kInitialYield:
      return {true, AFuncArgs, {inst.NumOperands()}, AAny};
    case Opcode::kYieldValue:
      return {true, AFuncArgs, {1, 1}, AAny};

    // YieldFrom's output is either the yielded value from the subiter or the
    // final result from a StopIteration, and is owned in either case.
    case Opcode::kYieldFrom: {
      return commonEffects(inst, AAny);
    }
    // YieldAndYieldFrom is equivalent to YieldFrom composed with YieldValue,
    // and steals the value it yields to the caller.
    case Opcode::kYieldAndYieldFrom: {
      return {false, AEmpty, {2, 1}, AAny};
    }

    case Opcode::kCast: {
      auto& cast = static_cast<const Cast&>(inst);
      if (cast.iserror()) {
        return commonEffects(inst, AEmpty);
      }
      // returns borrowed ref to Py_True/Py_False
      return borrowFrom(inst, AGlobal);
    }

    case Opcode::kCallCFunc:
      return commonEffects(inst, AManagedHeapAny);

    case Opcode::kBranch:
    case Opcode::kCondBranch:
    case Opcode::kCondBranchIterNotDone:
    case Opcode::kCondBranchCheckType:
    case Opcode::kPhi:
      JIT_CHECK(
          false,
          "Opcode %s doesn't have well-defined memory effects",
          inst.opname());
    case Opcode::kGetTuple:
      return commonEffects(inst, AAny);
  }

  JIT_CHECK(false, "Bad opcode %d", static_cast<int>(inst.opcode()));
}

} // namespace hir
} // namespace jit
