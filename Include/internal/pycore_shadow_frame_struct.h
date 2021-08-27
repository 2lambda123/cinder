/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#ifndef Py_SHADOW_FRAME_STRUCT_H
#define Py_SHADOW_FRAME_STRUCT_H

#include <stdint.h>

/*
 * Shadow frames are an optimization used by the JIT to avoid allocating
 * PyFrameObjects unless absolutely necessary (e.g. when a user calls
 * sys._getframe()).
 *
 * Shadow frames are allocated by both the interpreter and JIT-compiled
 * functions either on the system stack or in generator object instances and
 * linked into a call stack with the top linked to in PyThreadState.
 *
 * When a user requests a Python frame for a JIT-compiled function, the runtime
 * will allocate one and insert it into the appropriate place in chain of
 * PyFrameObjects. If the JIT-compiled function corresponded to a generator, the
 * newly allocated PyFrameObject will be linked to the corresponding generator
 * for the rest of its execution.
 *
 * In addition to allowing materialization of PyFrameObjects, shadow frames
 * provide enough information for introspection of the PyCodeObject's for all
 * active functions in the current call-stack.
 *
 * For stack introspection, we'll want to walk either the synchronous call
 * stack or the "await stack" and retrieve the PyCodeObject for each member.
 * The synchronous call stack is represented by the linked-list of shadow
 * frames that begins at the top-most shadow frame of the current thread.
 * The "await stack" consists of the chain of coroutines that are
 * transitively awaiting on the top-most coroutine of the current
 * thread. This chain is threaded through the coroutine object; to recover it
 * from a shadow frame, we must be able to go from a shadow frame to its
 * associated coroutine object. To do this we take advantage of shadow frames
 * for generator-like functions being stored within the associated PyGenObject.
 * Thus we can recover a pointer of the PyGenObject at a fixed offset from a
 * shadow frame pointer. We can use other data in the shadow frame to determine
 * if it refers to a generator function and so such a translation is valid.
 */
typedef struct _PyShadowFrame {
  struct _PyShadowFrame *prev;

  /*
   * This data field holds a pointer in the upper bits and meta-data in the
   * lower bits. The format is as follows:
   *
   *   [pointer: void*][pointer_kind: _PyShadowFrame_PtrKind]
   *    62 bits         2 bits
   *
   * The contents of `pointer` depends on the value of `pointer_kind`. See below
   * in the definition of _PyShadowFrame_PtrKind for details. A full 64 bit
   * pointer takes the 63 bits with the bottom bits padded with zeros.
   */
  uintptr_t data;
} _PyShadowFrame;

typedef enum {
  /* Pointer holds jit::CodeRuntime*. The frame refers to a JIT function which
   * is sufficient to reify a PyFrameObject, access a PyCodeObject, or tell if
   * the function is a generator. */
  PYSF_CODE_RT = 0b00,

  /* Pointer holds PyFrameObject*. */
  PYSF_PYFRAME = 0b01,

  /* Pointer holds PyCodeObject*. */
  PYSF_PYCODE = 0b10,

  /* Dummy value. The JIT assumes that a PtrKind has bit 0 set if any only if
   * data is a PyFrameObject*, so this value should be skipped if we add more
   * kinds. */
  PYSF_DUMMY = 0b11,
} _PyShadowFrame_PtrKind;

#endif /* !Py_SHADOW_FRAME_STRUCT_H */
