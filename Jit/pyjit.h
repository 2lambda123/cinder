// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#ifndef Py_JIT_H
#define Py_JIT_H

#include "Python.h"
#include "frameobject.h"
#include "genobject.h"

#include "Jit/pyjit_result.h"
#include "Jit/pyjit_typeslots.h"

// Offset of the state field in jit::GenFooterData for fast access from C code.
// This value is verified by static_assert in runtime.h.
#define _PY_GEN_JIT_DATA_STATE_OFFSET 24

#ifndef Py_LIMITED_API
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Offset of the code object within a jit::CodeRuntime
 */
PyAPI_DATA(int64_t) __strobe_CodeRuntime_py_code;

/*
 * This defines the global public API for the JIT that is consumed by the
 * runtime.
 *
 * These methods assume that the GIL is held unless it is explicitly stated
 * otherwise.
 */

/*
 * Initialize any global state required by the JIT.
 *
 * This must be called before attempting to use the JIT.
 *
 * Returns 0 on success or -1 on error.
 */
PyAPI_FUNC(int) _PyJIT_Initialize(void);

/*
 * Enable the global JIT.
 *
 * _PyJIT_Initialize must be called before calling this.
 *
 * Returns 1 if the JIT is enabled and 0 otherwise.
 */
PyAPI_FUNC(int) _PyJIT_Enable(void);

/*
 * Disable the global JIT.
 */
PyAPI_FUNC(void) _PyJIT_Disable(void);

/*
 * Returns 1 if JIT compilation is enabled and 0 otherwise.
 */
PyAPI_FUNC(int) _PyJIT_IsEnabled(void);

/*
 * After-fork callback for child processes. Performs any cleanup necessary for
 * per-process state, including handling of Linux perf pid maps.
 */
PyAPI_FUNC(void) _PyJIT_AfterFork_Child(void);

/*
 * Enable type slot specialization.
 */
PyAPI_FUNC(int) _PyJIT_EnableTypeSlots(void);

/*
 * Returns 1 if type slot specialization is enabled and 0 otherwise.
 */
PyAPI_FUNC(int) _PyJIT_AreTypeSlotsEnabled(void);

/*
 * JITs slot functions for the type object, and handles setting up
 * deoptimization support for the type. The caller provides the type object and
 * a struct containing the pointers to the current slot functions.
 *
 * Returns PYJIT_RESULT_OK if all operations succeed, or one of the error codes
 * on failure.
 */
PyAPI_FUNC(_PyJIT_Result)
    _PyJIT_SpecializeType(PyTypeObject* type, _PyJIT_TypeSlots* slots);

/*
 * JIT compile func and patch its entry point.
 *
 * On success, positional only calls to func will use the JIT compiled version.
 *
 * Returns PYJIT_RESULT_OK on success.
 */
PyAPI_FUNC(_PyJIT_Result) _PyJIT_CompileFunction(PyFunctionObject* func);

/*
 * Registers a function with the JIT to be compiled in the future.
 *
 * The JIT will still be informed by _PyJIT_CompileFunction before the
 * function executes for the first time.  The JIT can choose to compile
 * the function at some future point.  Currently the JIT will compile
 * the function before it shuts down to make sure all eligable functions
 * were compiled.
 *
 * The JIT will not keep the function alive, instead it will be informed
 * that the function is being de-allocated via _PyJIT_UnregisterFunction
 * before the function goes away.
 *
 * Returns 1 if the function is registered with JIT or is already compiled,
 * and 0 otherwise.
 */
PyAPI_FUNC(int) _PyJIT_RegisterFunction(PyFunctionObject* func);

/*
 * Informs the JIT that a type, function, or code object is being modified or
 * de-allocated.
 */
PyAPI_FUNC(void) _PyJIT_TypeModified(PyTypeObject* type);
PyAPI_FUNC(void) _PyJIT_TypeDestroyed(PyTypeObject* type);
PyAPI_FUNC(void) _PyJIT_FuncModified(PyFunctionObject* func);
PyAPI_FUNC(void) _PyJIT_FuncDestroyed(PyFunctionObject* func);
PyAPI_FUNC(void) _PyJIT_CodeDestroyed(PyCodeObject* code);

/*
 * Clean up any resources allocated by the JIT.
 *
 * This is intended to be called at interpreter shutdown in Py_Finalize.
 *
 * Returns 0 on success or -1 on error.
 */
PyAPI_FUNC(int) _PyJIT_Finalize(void);

/*
 * Returns whether the function specified in `func` is on the jit-list.
 *
 * Returns 0 if the given function is not on the jit-list, and non-zero
 * otherwise.
 *
 * Always returns 1 if the JIT list is not specified.
 */
PyAPI_FUNC(int) _PyJIT_OnJitList(PyFunctionObject* func);

/*
 * Returns a boolean indicating whether or not jitted functions should use a
 * shadow frame object by default instead of a full PyFrameObject.
 */
PyAPI_FUNC(int) _PyJIT_ShadowFrame(void);

/* Dict-watching callbacks, invoked by dictobject.c when appropriate. */

/*
 * Called when the value at a key is modified (value will contain the new
 * value) or deleted (value will be NULL).
 */
PyAPI_FUNC(void)
    _PyJIT_NotifyDictKey(PyObject* dict, PyObject* key, PyObject* value);

/*
 * Called when a dict is cleared, rather than sending individual notifications
 * for every key. The dict is still in a watched state, and further callbacks
 * for it will be invoked as appropriate.
 */
PyAPI_FUNC(void) _PyJIT_NotifyDictClear(PyObject* dict);

/*
 * Called when a dict has changed in a way that is incompatible with watching,
 * or is about to be freed. No more callbacks will be invoked for this dict.
 */
PyAPI_FUNC(void) _PyJIT_NotifyDictUnwatch(PyObject* dict);

/*
 * Gets the global cache for the given globals dictionary and key.  The global
 * that is pointed to will automatically be updated as builtins and globals
 * change.  The value that is pointed to will be NULL if the dictionaries can
 * no longer be tracked or if the value is no longer defined, in which case
 * the dictionaries need to be consulted.  This will return NULL if the required
 * tracking cannot be initialized.
 */
PyAPI_FUNC(PyObject**) _PyJIT_GetGlobalCache(PyObject* globals, PyObject* key);

/*
 * Gets the cache for the given dictionary and key.  The value that is pointed
 * to will automatically be updated as the dictionary changes.  The value that
 * is pointed to will be NULL if the dictionaries can no longer be tracked or if
 * the value is no longer defined, in which case the dictionaries need to be
 * consulted.  This will return NULL if the required tracking cannot be
 * initialized.
 */
PyAPI_FUNC(PyObject**) _PyJIT_GetDictCache(PyObject* dict, PyObject* key);

/*
 * Clears internal caches associated with the JIT.  This may cause a degradation
 * of performance and is only intended for use for detecting memory leaks.
 */
PyAPI_FUNC(void) _PyJIT_ClearDictCaches(void);

/*
 * Send into/resume a suspended JIT generator and return the result.
 */
PyAPI_FUNC(PyObject*) _PyJIT_GenSend(
    PyGenObject* gen,
    PyObject* arg,
    int exc,
    PyFrameObject* f,
    PyThreadState* tstate,
    int finish_yield_from);

/*
 * Materialize the frame for gen. Returns a new reference.
 */
PyAPI_FUNC(PyFrameObject*) _PyJIT_GenMaterializeFrame(PyGenObject* gen);

/*
 * Visit owned references in a JIT-backed generator object.
 */
PyAPI_FUNC(int)
    _PyJIT_GenVisitRefs(PyGenObject* gen, visitproc visit, void* arg);

/*
 * Release any JIT-related data in a PyGenObject.
 */
PyAPI_FUNC(void) _PyJIT_GenDealloc(PyGenObject* gen);

/*
 * Extract overall JIT state for generator. Implemented as a macro for perf
 * in C code that doesn't have access to C++ types.
 */
#define _PyJIT_GenState(gen) \
  ((_PyJitGenState)((char*)gen->gi_jit_data)[_PY_GEN_JIT_DATA_STATE_OFFSET])

/*
 * Return current sub-iterator from JIT generator or NULL if there is none.
 */
PyAPI_FUNC(PyObject*) _PyJIT_GenYieldFromValue(PyGenObject* gen);

/*
 * Specifies the offset from a JITed function entry point where the re-entry
 * point for calling with the correct bound args lives */
#define JITRT_CALL_REENTRY_OFFSET (-6)

/*
 * Fixes the JITed function entry point up to be the re-entry point after
 * binding the args */
#define JITRT_GET_REENTRY(entry) \
  ((vectorcallfunc)(((char*)entry) + JITRT_CALL_REENTRY_OFFSET))

/*
 * Specifies the offset from a JITed function entry point where the static
 * entry point lives */
#define JITRT_STATIC_ENTRY_OFFSET (-8)

/*
 * Fixes the JITed function entry point up to be the static entry point after
 * binding the args */
#define JITRT_GET_STATIC_ENTRY(entry) \
  ((vectorcallfunc)(((char*)entry) + JITRT_STATIC_ENTRY_OFFSET))

/*
 * Checks if the given function is JITed.

 * Returns 1 if the function is JITed, 0 if not.
 */
PyAPI_FUNC(int) _PyJIT_IsCompiled(PyObject* func);

/*
 * Returns a borrowed reference to the globals for the top-most Python function
 * associated with tstate.
 */
PyAPI_FUNC(PyObject*) _PyJIT_GetGlobals(PyThreadState* tstate);

/*
 * Indicates whether or not newly-created interpreter threads should have type
 * profiling enabled by default.
 */
extern int g_profile_new_interp_threads;

/*
 * Record a type profile for the current instruction.
 */
PyAPI_FUNC(void) _PyJIT_ProfileCurrentInstr(
    PyFrameObject* frame,
    PyObject** stack_top,
    int opcode,
    int oparg);

/*
 * Record profiled instructions for the given code object upon exit from a
 * frame, some of which may not have had their types recorded.
 */
PyAPI_FUNC(void)
    _PyJIT_CountProfiledInstrs(PyCodeObject* code, Py_ssize_t count);

/*
 * Get and clear, or just clear, information about the recorded type profiles.
 */
PyAPI_FUNC(PyObject*) _PyJIT_GetAndClearTypeProfiles(void);
PyAPI_FUNC(void) _PyJIT_ClearTypeProfiles(void);

/*
 * Notify the JIT that type has been modified.
 */
PyAPI_FUNC(void) _PyJIT_TypeModified(PyTypeObject* type);

#ifdef __cplusplus
}
#endif
#endif /* Py_LIMITED_API */
#endif /* Py_JIT_H */
