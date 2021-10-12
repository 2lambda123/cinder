// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#ifndef Py_JIT_GDB_SUPPORT_H
#define Py_JIT_GDB_SUPPORT_H

#include "Python.h"

namespace jit {
class CompiledFunction;
}

extern int g_gdb_support;
extern int g_gdb_write_elf_objects;
extern int g_gdb_stubs_support;

int gdb_support_enabled(void);

int register_raw_debug_symbol(
    const char* function_name,
    const char* filename,
    int lineno,
    void* code_addr,
    size_t code_size,
    size_t stack_size);

int register_pycode_debug_symbol(
    PyCodeObject* codeobj,
    const char* fullname,
    jit::CompiledFunction* compiled_func);

#endif // Py_JIT_GDB_SUPPORT_H
