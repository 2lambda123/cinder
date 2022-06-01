// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#pragma once

#ifndef Py_LIMITED_API
#ifdef __cplusplus
extern "C" {
#endif

/* Status codes for the result of JIT attempts. */
typedef enum {
  PYJIT_RESULT_OK,

  /*
   * We cannot specialize the input.
   *
   * For example, we cannot generate a specialized tp_init slot if the __init__
   * method of the class is not a function.
   */
  PYJIT_RESULT_CANNOT_SPECIALIZE,

  /* Someone tried to compile a function but the JIT is not initialized. */
  PYJIT_NOT_INITIALIZED,

  /* During threaded compile we may end compiling the same code twice in
     different contexts. If you get this response, you should retry later
     or give up as best fits the case. */
  PYJIT_RESULT_RETRY,

  /* JIT list is enabled and this function is not on it. */
  PYJIT_RESULT_NOT_ON_JITLIST,

  /* No preloader is available for this function. Compilation may work later. */
  PYJIT_RESULT_NO_PRELOADER,

  PYJIT_RESULT_UNKNOWN_ERROR
} _PyJIT_Result;

#ifdef __cplusplus
}
#endif
#endif /* Py_LIMITED_API */
