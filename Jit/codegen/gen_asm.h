// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#pragma once

#include "Python.h"
#include "opcode.h"

#include "Jit/bitvector.h"
#include "Jit/codegen/environ.h"
#include "Jit/codegen/x86_64.h"
#include "Jit/hir/hir.h"
#include "Jit/jit_rt.h"
#include "Jit/lir/lir.h"
#include "Jit/pyjit.h"
#include "Jit/util.h"

#include <asmjit/asmjit.h>

#include <algorithm>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace jit {

namespace codegen {

// Generate the final stage trampoline that is responsible for finishing
// execution in the interpreter and then returning the result to the caller.
void* generateDeoptTrampoline(bool generator_mode);
void* generateJitTrampoline();

class NativeGenerator {
 public:
  NativeGenerator(const hir::Function* func)
      : func_(func),
        deopt_trampoline_(generateDeoptTrampoline(false)),
        deopt_trampoline_generators_(generateDeoptTrampoline(true)),
        jit_trampoline_(generateJitTrampoline()),
        frame_header_size_(calcFrameHeaderSize(func)),
        max_inline_depth_(calcMaxInlineDepth(func)) {
    env_.has_inlined_functions = max_inline_depth_ > 0;
  }

  NativeGenerator(
      const hir::Function* func,
      void* deopt_trampoline,
      void* deopt_trampoline_generators,
      void* jit_trampoline)
      : func_(func),
        deopt_trampoline_(deopt_trampoline),
        deopt_trampoline_generators_(deopt_trampoline_generators),
        jit_trampoline_(jit_trampoline),
        frame_header_size_(calcFrameHeaderSize(func)),
        max_inline_depth_(calcMaxInlineDepth(func)) {
    env_.has_inlined_functions = max_inline_depth_ > 0;
  }

  void SetJSONOutput(nlohmann::json* json) {
    JIT_CHECK(json != nullptr, "expected non-null stream");
    this->json = json;
  }

  ~NativeGenerator() {
    if (as_ != nullptr) {
      delete as_;
    }
  }

  std::string GetFunctionName() const;
  void* GetEntryPoint();
  int GetCompiledFunctionSize() const;
  int GetCompiledFunctionStackSize() const;
  int GetCompiledFunctionSpillStackSize() const;
  const hir::Function* GetFunction() const {
    return func_;
  }

  CodeRuntime* codeRuntime() const {
    return env_.code_rt;
  }

  bool isGen() const {
    return func_->code->co_flags & kCoFlagsAnyGenerator;
  }

#ifdef __ASM_DEBUG
  const char* GetPyFunctionName() const;
#endif
 private:
  const hir::Function* func_;
  void* entry_{nullptr};
  asmjit::x86::Builder* as_{nullptr};
  void* deopt_trampoline_{nullptr};
  void* deopt_trampoline_generators_{nullptr};
  void* jit_trampoline_{nullptr};

  int compiled_size_{-1};
  int spill_stack_size_{-1};
  int frame_header_size_;
  int max_inline_depth_;

  int calcFrameHeaderSize(const hir::Function* func);
  int calcMaxInlineDepth(const hir::Function* func);
  void generateCode(asmjit::CodeHolder& code);
  void generateFunctionEntry();
  void linkOnStackShadowFrame(
      asmjit::x86::Gp tstate_reg,
      asmjit::x86::Gp scratch_reg);
  void initializeFrameHeader(
      asmjit::x86::Gp tstate_reg,
      asmjit::x86::Gp scratch_reg);
  int setupFrameAndSaveCallerRegisters(asmjit::x86::Gp tstate_reg);
  void generatePrologue(
      asmjit::Label correct_arg_count,
      asmjit::Label native_entry_point);
  void loadOrGenerateLinkFrame(
      asmjit::x86::Gp tstate_reg,
      const std::vector<
          std::pair<const asmjit::x86::Reg&, const asmjit::x86::Reg&>>&
          save_regs);
  void generateEpilogue(asmjit::BaseNode* epilogue_cursor);
  void generateEpilogueUnlinkFrame(asmjit::x86::Gp tstate_reg, bool is_gen);
  void generateDeoptExits();
  void linkDeoptPatchers(const asmjit::CodeHolder& code);
  void linkIPtoBCMappings(const asmjit::CodeHolder& code);
  void generateResumeEntry();
  void generateStaticMethodTypeChecks(asmjit::Label setup_frame);
  void generateStaticEntryPoint(
      asmjit::Label native_entry_point,
      asmjit::Label static_jmp_location);
  void generateTypedArgumentInfo();
  void loadTState(asmjit::x86::Gp dst_reg);

  void CollectOptimizableLoadMethods();

  FRIEND_TEST(LinearScanAllocatorTest, RegAllocation);
  friend class BackendTest;

  // XXX: the following are functions/variables for the new assembly generator.
  // after it is complete and fully tested, need to remove the old generator.
  void generateAssemblyBody();

  bool isPredefinedUsed(const char* name);

  std::unique_ptr<lir::Function> lir_func_;
  Environ env_;
  nlohmann::json* json{nullptr};
};

class NativeGeneratorFactory {
 public:
  NativeGeneratorFactory() {
    deopt_trampoline_ = generateDeoptTrampoline(false);
    deopt_trampoline_generators_ = generateDeoptTrampoline(true);
    jit_trampoline_ = generateJitTrampoline();
  }

  static Runtime* runtime() {
    if (s_jit_asm_code_rt_ == nullptr) {
      s_jit_asm_code_rt_ = new Runtime;
    }
    return s_jit_asm_code_rt_;
  }

  std::unique_ptr<NativeGenerator> operator()(const hir::Function* func) const {
    return std::make_unique<NativeGenerator>(
        func,
        deopt_trampoline_,
        deopt_trampoline_generators_,
        jit_trampoline_);
  }

  static void shutdown() {
    _PyJIT_ClearDictCaches();
    delete s_jit_asm_code_rt_;
    s_jit_asm_code_rt_ = nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(NativeGeneratorFactory);

 private:
  static Runtime* s_jit_asm_code_rt_;

  void* deopt_trampoline_;
  void* deopt_trampoline_generators_;
  void* jit_trampoline_;
};

// Returns whether or not we can load/store reg from/to addr with a single
// instruction.
bool canLoadStoreAddr(asmjit::x86::Gp reg, int64_t addr);

} // namespace codegen
} // namespace jit
