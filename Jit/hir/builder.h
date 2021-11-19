// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#pragma once

#include "Python.h"

#include "Jit/bytecode.h"
#include "Jit/hir/hir.h"
#include "Jit/hir/preload.h"
#include "Jit/stack.h"
#include "Jit/util.h"

#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace jit {

class BytecodeInstruction;

namespace hir {

class BasicBlock;
class Environment;
class Function;
class Register;

extern const std::unordered_set<int> kSupportedOpcodes;

// Helper class for managing temporary variables
class TempAllocator {
 public:
  explicit TempAllocator(Environment* env) : env_(env) {}

  // Allocate a temp register that may be used for the stack. It should not be a
  // register that will be treated specially in the FrameState (e.g. tracked as
  // containing a local or cell.)
  Register* AllocateStack();

  // Get the i-th stack temporary or allocate one
  Register* GetOrAllocateStack(std::size_t idx);

  // Allocate a temp register that will not be used for a stack value.
  Register* AllocateNonStack();

 private:
  Environment* env_;
  std::vector<Register*> cache_;
};

// We expect that on exit from a basic block the stack only contains temporaries
// in increasing order (called the canonical form). For example,
//
//    t0
//    t1
//    t2 <- top of stack
//
// It may be the case that temporaries are re-ordered, duplicated, or the stack
// contains locals. This class is responsible for inserting the necessary
// register moves such that the stack is in canonical form.
class BlockCanonicalizer {
 public:
  BlockCanonicalizer() : processing_(), done_(), copies_(), moved_() {}

  void Run(BasicBlock* block, TempAllocator& temps, OperandStack& stack);

 private:
  DISALLOW_COPY_AND_ASSIGN(BlockCanonicalizer);

  void InsertCopies(
      Register* reg,
      TempAllocator& temps,
      Instr& terminator,
      std::vector<Register*>& alloced);

  std::unordered_set<Register*> processing_;
  std::unordered_set<Register*> done_;
  std::unordered_map<Register*, std::vector<Register*>> copies_;
  std::unordered_map<Register*, Register*> moved_;
};

// Convenience wrapper, used only in tests
std::unique_ptr<Function> buildHIR(BorrowedRef<PyFunctionObject> func);

// Translate the bytecode for preloader.code into HIR, in the context of the
// preloaded globals and classloader lookups in the preloader.
//
// The resulting HIR is un-optimized, not in SSA form, and does not yet have
// refcount operations or types flowed through it. Later passes will transform
// to SSA, flow types, optimize, and insert refcount operations using liveness
// analysis.
std::unique_ptr<Function> buildHIR(const Preloader& preloader);

class HIRBuilder {
 public:
  HIRBuilder(const Preloader& preloader)
      : code_(preloader.code()), preloader_(preloader){};

  // Translate the bytecode for code_ into HIR, in the context of the preloaded
  // globals and classloader lookups from preloader_.
  //
  // The resulting HIR is un-optimized, not in SSA form, and does not yet have
  // refcount operations or types flowed through it. Later passes will transform
  // to SSA, flow types, optimize, and insert refcount operations using liveness
  // analysis.
  //
  // TODO(mpage): Consider using something like Either here to indicate reason
  // for failure.
  std::unique_ptr<Function> buildHIR();

 private:
  DISALLOW_COPY_AND_ASSIGN(HIRBuilder);

  struct TranslationContext;
  // Completes compilation of a finally block
  using FinallyCompleter =
      std::function<void(TranslationContext&, const jit::BytecodeInstruction&)>;
  void translate(
      Function& irfunc,
      const jit::BytecodeInstructionBlock& bc_instrs,
      const TranslationContext& tc,
      FinallyCompleter complete_finally = nullptr);

  void emitBinaryOp(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitUnaryOp(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitAnyCall(
      CFG& cfg,
      TranslationContext& tc,
      jit::BytecodeInstructionBlock::Iterator& bc_it,
      const jit::BytecodeInstructionBlock& bc_instrs);
  void emitCallEx(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool is_awaited);
  void emitCallFunction(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool is_awaited);
  void emitCallKWArgs(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool is_awaited);
  void emitCallMethod(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool is_awaited);
  void emitCompareOp(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitJumpIf(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitDeleteAttr(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadAttr(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadMethod(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadMethodOrAttrSuper(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool load_method);
  void emitLoadDeref(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitStoreDeref(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadConst(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadFast(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadGlobal(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadType(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitMakeFunction(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitMakeListTuple(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitMakeListTupleUnpack(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBuildCheckedList(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBuildCheckedMap(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBuildMap(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBuildMapUnpack(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool with_call);
  void emitBuildSet(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBuildSetUnpack(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBuildConstKeyMap(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitPopJumpIf(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitStoreAttr(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitStoreFast(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitStoreSubscr(TranslationContext& tc);
  void emitInPlaceOp(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBuildSlice(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadIterableArg(
      CFG& cfg,
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  bool emitInvokeFunction(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool is_awaited);
  void emitGetIter(TranslationContext& tc);
  void emitGetYieldFromIter(CFG& cfg, TranslationContext& tc);
  void emitListAppend(
      TranslationContext& tc,
      const BytecodeInstruction& bc_instr);
  void emitForIter(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  bool emitInvokeMethod(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      bool is_awaited);
  void emitInvokeTypedMethod(
      TranslationContext& tc,
      PyMethodDef* method,
      Py_ssize_t nargs);
  void emitLoadField(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitStoreField(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitCast(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitTpAlloc(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitStoreLocal(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitLoadLocal(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitConvertPrimitive(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitPrimitiveLoadConst(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitIntLoadConstOld(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitPrimitiveBinaryOp(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitPrimitiveCompare(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitPrimitiveBox(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitPrimitiveUnbox(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitImportFrom(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitImportName(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitPrimitiveUnaryOp(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitFastLen(
      CFG& cfg,
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitRaiseVarargs(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitRefineType(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitSequenceGet(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitSequenceRepeat(
      CFG& cfg,
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitSequenceSet(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitYieldValue(TranslationContext& tc);
  void emitGetAwaitable(CFG& cfg, TranslationContext& tc, int prev_op);
  void emitUnpackEx(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitUnpackSequence(
      CFG& cfg,
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitBeginFinally(
      Function& irfunc,
      TranslationContext& tc,
      const BytecodeInstructionBlock& bc_instrs,
      const jit::BytecodeInstruction& bc_instr,
      std::deque<TranslationContext>& queue);
  void emitCallFinally(
      Function& irfunc,
      TranslationContext& tc,
      const BytecodeInstructionBlock& bc_instrs,
      const jit::BytecodeInstruction& bc_instr,
      std::deque<TranslationContext>& queue);
  void emitEndFinally(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      FinallyCompleter complete_finally);
  void emitFinallyBlock(
      Function& irfunc,
      TranslationContext& tc,
      const BytecodeInstructionBlock& bc_instrs,
      std::deque<TranslationContext>& queue,
      Py_ssize_t finally_off,
      BasicBlock* ret_block);
  void emitPopFinally(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr,
      FinallyCompleter complete_finally);
  void emitSetupFinally(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitAsyncForHeaderYieldFrom(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitEndAsyncFor(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitGetAIter(TranslationContext& tc);
  void emitGetANext(TranslationContext& tc);
  Register* emitSetupWithCommon(
      TranslationContext& tc,
      _Py_Identifier* enter_id,
      _Py_Identifier* exit_id,
      bool swap_lookup);
  void emitBeforeAsyncWith(TranslationContext& tc);
  void emitSetupAsyncWith(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitSetupWith(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitWithCleanupStart(TranslationContext& tc);
  void emitWithCleanupFinish(TranslationContext& tc);
  void emitYieldFrom(TranslationContext& tc, Register* out);
  void emitDispatchEagerCoroResult(
      CFG& cfg,
      TranslationContext& tc,
      Register* out,
      BasicBlock* await_block,
      BasicBlock* post_await_block);

  void emitBuildString(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitFormatValue(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);

  void emitMapAdd(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);
  void emitSetAdd(
      TranslationContext& tc,
      const jit::BytecodeInstruction& bc_instr);

  BorrowedRef<> constArg(const jit::BytecodeInstruction& bc_instr);

  ExecutionBlock popBlock(CFG& cfg, TranslationContext& tc);
  void insertEvalBreakerCheckForLoop(CFG& cfg, BasicBlock* loop_header);
  void insertEvalBreakerCheckForExcept(CFG& cfg, TranslationContext& tc);
  void insertEvalBreakerCheck(
      CFG& cfg,
      BasicBlock* check_block,
      BasicBlock* succ,
      const FrameState& frame);
  void addInitialYield(TranslationContext& tc);
  void addLoadArgs(TranslationContext& tc, int num_args);
  void addInitializeCells(TranslationContext& tc, Register* cur_func);
  void AllocateRegistersForLocals(Environment* env, FrameState& state);
  void AllocateRegistersForCells(Environment* env, FrameState& state);
  void moveOverwrittenStackRegisters(TranslationContext& tc, Register* dst);
  bool tryEmitDirectMethodCall(
      const InvokeTarget& target,
      TranslationContext& tc,
      long nargs);
  struct BlockMap {
    std::unordered_map<Py_ssize_t, BasicBlock*> blocks;
    std::unordered_map<BasicBlock*, BytecodeInstructionBlock> bc_blocks;
  };
  BlockMap createBlocks(
      Function& irfunc,
      const BytecodeInstructionBlock& bc_block);
  BasicBlock* getBlockAtOff(Py_ssize_t off);

  BorrowedRef<PyCodeObject> code_;
  BlockMap block_map_;
  const Preloader& preloader_;

  // Map index of END_ASYNC_FOR bytecodes to FrameState of paired YIELD_FROMs
  std::unordered_map<size_t, FrameState> end_async_for_frame_state_;

  TempAllocator temps_{nullptr};
};

} // namespace hir
} // namespace jit
