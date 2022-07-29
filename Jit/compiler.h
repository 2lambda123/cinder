// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#pragma once

#include "Python.h"

#include "Jit/codegen/gen_asm.h"
#include "Jit/hir/hir.h"
#include "Jit/hir/preload.h"
#include "Jit/runtime.h"
#include "Jit/util.h"

namespace jit {

// CompiledFunction contains the native code that was compiled for a python
// function
//
// It is responsible for managing the lifetime of the executable memory and
// binding the lifetime of anything it depends on to it.
class CompiledFunction {
 public:
  CompiledFunction(
      vectorcallfunc entry,
      CodeRuntime* code_runtime,
      int func_size,
      int stack_size,
      int spill_stack_size,
      int num_inlined_functions)
      : entry_point_(entry),
        code_runtime_(code_runtime),
        code_size_(func_size),
        stack_size_(stack_size),
        spill_stack_size_(spill_stack_size),
        num_inlined_functions_(num_inlined_functions) {}

  virtual ~CompiledFunction() {}

  vectorcallfunc entry_point() const {
    return entry_point_;
  }

  PyObject* Invoke(PyObject* func, PyObject** args, Py_ssize_t nargs) {
    return entry_point_(func, args, nargs, NULL);
  }

  virtual void PrintHIR() const;
  virtual void Disassemble() const;

  CodeRuntime* codeRuntime() const {
    return code_runtime_;
  }

  int GetCodeSize() const {
    return code_size_;
  }
  int GetStackSize() const {
    return stack_size_;
  }
  int GetSpillStackSize() const {
    return spill_stack_size_;
  }
  int GetNumInlinedFunctions() const {
    return num_inlined_functions_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompiledFunction);

  vectorcallfunc entry_point_;
  CodeRuntime* code_runtime_{nullptr};
  int code_size_;
  int stack_size_;
  int spill_stack_size_;
  int num_inlined_functions_;
};

// same as CompiledFunction class but keeps HIR and LIR classes for debug
// purposes
class CompiledFunctionDebug : public CompiledFunction {
 public:
  CompiledFunctionDebug(
      vectorcallfunc entry,
      CodeRuntime* code_runtime,
      int func_size,
      int stack_size,
      int spill_stack_size,
      int num_inlined_functions,
      std::unique_ptr<hir::Function> irfunc,
      std::unique_ptr<codegen::NativeGenerator> ngen)
      : CompiledFunction(
            entry,
            code_runtime,
            func_size,
            stack_size,
            spill_stack_size,
            num_inlined_functions),
        irfunc_(std::move(irfunc)),
        ngen_(std::move(ngen)) {}

  void Disassemble() const override;
  void PrintHIR() const override;

 private:
  std::unique_ptr<hir::Function> irfunc_;
  std::unique_ptr<codegen::NativeGenerator> ngen_;
};

typedef std::function<
    void(hir::Function& func, const char* pass_name, std::size_t time_ns)>
    PostPassFunction;

enum PassConfig : uint64_t {
  kDefault = 0,
  kEnableHIRInliner = 1 << 0,
};

// Compiler is the high-level interface for translating Python functions into
// native code.
class Compiler {
 public:
  Compiler() = default;

  // Compile the function / code object preloaded by the given Preloader.
  std::unique_ptr<CompiledFunction> Compile(const hir::Preloader& preloader);

  // Convenience wrapper to create and compile a preloader from a
  // PyFunctionObject.
  std::unique_ptr<CompiledFunction> Compile(BorrowedRef<PyFunctionObject> func);

  // Runs all the compiler passes on the HIR function.
  static void runPasses(hir::Function&, PassConfig config);
  // Runs the compiler passes, calling callback on the HIR function after each
  // pass.
  static void runPasses(
      hir::Function& irfunc,
      PassConfig config,
      PostPassFunction callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(Compiler);
  codegen::NativeGeneratorFactory ngen_factory_;
};

} // namespace jit
