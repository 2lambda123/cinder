/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */

#include "Python.h"
#include "boolobject.h"
#include "dictobject.h"
#include "funcobject.h"
#include "import.h"
#include "methodobject.h"
#include "object.h"
#include "pyport.h"
#include "structmember.h"
#include "pycore_object.h"
#include "classloader.h"

PyDoc_STRVAR(_static__doc__,
             "_static contains types related to static Python\n");

extern PyTypeObject _PyCheckedDict_Type;
extern PyTypeObject _PyCheckedList_Type;

static int
_static_exec(PyObject *m)
{
    if (PyType_Ready((PyTypeObject *)&_PyCheckedDict_Type) < 0)
        return -1;

    if (PyType_Ready((PyTypeObject *)&_PyCheckedList_Type) < 0)
        return -1;

    PyObject *globals = ((PyStrictModuleObject *)m)->globals;

    if (PyDict_SetItemString(globals, "chkdict", (PyObject *)&_PyCheckedDict_Type) < 0)
        return -1;

    if (PyDict_SetItemString(globals, "chklist", (PyObject *)&_PyCheckedList_Type) < 0)
        return -1;
    PyObject *type_code;
#define SET_TYPE_CODE(name)                                           \
    type_code = PyLong_FromLong(name);                                \
    if (type_code == NULL) {                                          \
        return -1;                                                    \
    }                                                                 \
    if (PyDict_SetItemString(globals, #name, type_code) < 0) {        \
        Py_DECREF(type_code);                                         \
        return -1;                                                    \
    }                                                                 \
    Py_DECREF(type_code);

    SET_TYPE_CODE(TYPED_INT_UNSIGNED)
    SET_TYPE_CODE(TYPED_INT_SIGNED)
    SET_TYPE_CODE(TYPED_INT_8BIT)
    SET_TYPE_CODE(TYPED_INT_16BIT)
    SET_TYPE_CODE(TYPED_INT_32BIT)
    SET_TYPE_CODE(TYPED_INT_64BIT)
    SET_TYPE_CODE(TYPED_OBJECT)
    SET_TYPE_CODE(TYPED_ARRAY)
    SET_TYPE_CODE(TYPED_INT8)
    SET_TYPE_CODE(TYPED_INT16)
    SET_TYPE_CODE(TYPED_INT32)
    SET_TYPE_CODE(TYPED_INT64)
    SET_TYPE_CODE(TYPED_UINT8)
    SET_TYPE_CODE(TYPED_UINT16)
    SET_TYPE_CODE(TYPED_UINT32)
    SET_TYPE_CODE(TYPED_UINT64)
    SET_TYPE_CODE(TYPED_SINGLE)
    SET_TYPE_CODE(TYPED_DOUBLE)
    SET_TYPE_CODE(TYPED_BOOL)
    SET_TYPE_CODE(TYPED_CHAR)


    SET_TYPE_CODE(SEQ_LIST)
    SET_TYPE_CODE(SEQ_TUPLE)
    SET_TYPE_CODE(SEQ_LIST_INEXACT)
    SET_TYPE_CODE(SEQ_ARRAY_INT8)
    SET_TYPE_CODE(SEQ_ARRAY_INT16)
    SET_TYPE_CODE(SEQ_ARRAY_INT32)
    SET_TYPE_CODE(SEQ_ARRAY_INT64)
    SET_TYPE_CODE(SEQ_ARRAY_UINT8)
    SET_TYPE_CODE(SEQ_ARRAY_UINT16)
    SET_TYPE_CODE(SEQ_ARRAY_UINT32)
    SET_TYPE_CODE(SEQ_ARRAY_UINT64)
    SET_TYPE_CODE(SEQ_SUBSCR_UNCHECKED)

    SET_TYPE_CODE(SEQ_REPEAT_INEXACT_SEQ)
    SET_TYPE_CODE(SEQ_REPEAT_INEXACT_NUM)
    SET_TYPE_CODE(SEQ_REPEAT_REVERSED)
    SET_TYPE_CODE(SEQ_REPEAT_PRIMITIVE_NUM)

    SET_TYPE_CODE(SEQ_CHECKED_LIST)

    SET_TYPE_CODE(PRIM_OP_EQ_INT)
    SET_TYPE_CODE(PRIM_OP_NE_INT)
    SET_TYPE_CODE(PRIM_OP_LT_INT)
    SET_TYPE_CODE(PRIM_OP_LE_INT)
    SET_TYPE_CODE(PRIM_OP_GT_INT)
    SET_TYPE_CODE(PRIM_OP_GE_INT)
    SET_TYPE_CODE(PRIM_OP_LT_UN_INT)
    SET_TYPE_CODE(PRIM_OP_LE_UN_INT)
    SET_TYPE_CODE(PRIM_OP_GT_UN_INT)
    SET_TYPE_CODE(PRIM_OP_GE_UN_INT)
    SET_TYPE_CODE(PRIM_OP_EQ_DBL)
    SET_TYPE_CODE(PRIM_OP_NE_DBL)
    SET_TYPE_CODE(PRIM_OP_LT_DBL)
    SET_TYPE_CODE(PRIM_OP_LE_DBL)
    SET_TYPE_CODE(PRIM_OP_GT_DBL)
    SET_TYPE_CODE(PRIM_OP_GE_DBL)

    SET_TYPE_CODE(PRIM_OP_ADD_INT)
    SET_TYPE_CODE(PRIM_OP_SUB_INT)
    SET_TYPE_CODE(PRIM_OP_MUL_INT)
    SET_TYPE_CODE(PRIM_OP_DIV_INT)
    SET_TYPE_CODE(PRIM_OP_DIV_UN_INT)
    SET_TYPE_CODE(PRIM_OP_MOD_INT)
    SET_TYPE_CODE(PRIM_OP_MOD_UN_INT)
    SET_TYPE_CODE(PRIM_OP_LSHIFT_INT)
    SET_TYPE_CODE(PRIM_OP_RSHIFT_INT)
    SET_TYPE_CODE(PRIM_OP_RSHIFT_UN_INT)
    SET_TYPE_CODE(PRIM_OP_XOR_INT)
    SET_TYPE_CODE(PRIM_OP_OR_INT)
    SET_TYPE_CODE(PRIM_OP_AND_INT)

    SET_TYPE_CODE(PRIM_OP_ADD_DBL)
    SET_TYPE_CODE(PRIM_OP_SUB_DBL)
    SET_TYPE_CODE(PRIM_OP_MUL_DBL)
    SET_TYPE_CODE(PRIM_OP_DIV_DBL)
    SET_TYPE_CODE(PRIM_OP_MOD_DBL)
    SET_TYPE_CODE(PROM_OP_POW_DBL)

    SET_TYPE_CODE(PRIM_OP_NEG_INT)
    SET_TYPE_CODE(PRIM_OP_INV_INT)
    SET_TYPE_CODE(PRIM_OP_NEG_DBL)

    SET_TYPE_CODE(FAST_LEN_INEXACT)
    SET_TYPE_CODE(FAST_LEN_LIST)
    SET_TYPE_CODE(FAST_LEN_DICT)
    SET_TYPE_CODE(FAST_LEN_SET)
    SET_TYPE_CODE(FAST_LEN_TUPLE)
    SET_TYPE_CODE(FAST_LEN_ARRAY)
    SET_TYPE_CODE(FAST_LEN_STR)

    /* Not actually a type code, but still an int */
    SET_TYPE_CODE(RAND_MAX);

    return 0;
}

static PyObject* _static_create(PyObject *spec, PyModuleDef *def) {

    PyObject *mod_dict = PyDict_New();
    if (mod_dict == NULL) {
        return NULL;
    }
    PyObject *args = PyTuple_New(1);
    if (args == NULL) {
        Py_DECREF(mod_dict);
        return NULL;
    }

    PyTuple_SET_ITEM(args, 0, mod_dict);


    PyObject *res = PyStrictModule_New(&PyStrictModule_Type, args, NULL);
    Py_DECREF(args);
    if (res == NULL) {
        return NULL;
    }

    PyObject *name = PyUnicode_FromString("_static");
    if (name == NULL) {
        Py_DECREF(res);
        return NULL;
    }

    PyObject *base_dict = PyDict_New();
    if(base_dict == NULL) {
        Py_DECREF(res);
        Py_DECREF(name);
        return NULL;
    }

    ((PyModuleObject*)res)->md_dict = base_dict;
    if (PyDict_SetItemString(mod_dict, "__name__", name) ||
       PyModule_AddObject(res, "__name__", name)) {
        Py_DECREF(res);
        Py_DECREF(name);
        return NULL;
    }
    return res;
}

static struct PyModuleDef_Slot _static_slots[] = {
    {Py_mod_create, _static_create},
    {Py_mod_exec, _static_exec},
    {0, NULL},
};


PyObject *set_type_code(PyObject *mod, PyObject *const *args, Py_ssize_t nargs) {
    PyTypeObject *type;
    Py_ssize_t code;
    if (!_PyArg_ParseStack(args, nargs, "O!n", &PyType_Type, &type, &code)) {
        return NULL;
    } else if (!(type->tp_flags & Py_TPFLAGS_HEAPTYPE)) {
        PyErr_SetString(PyExc_TypeError, "expected heap type");
        return NULL;
    }

    PyHeapType_CINDER_EXTRA(type)->type_code = code;
    Py_RETURN_NONE;
}

PyObject *is_type_static(PyObject *mod, PyObject *type) {
  PyTypeObject *pytype;
  if (!PyType_Check(type)) {
    Py_RETURN_FALSE;
  }
  pytype = (PyTypeObject *)type;
  if (pytype->tp_flags & Py_TPFLAGS_IS_STATICALLY_DEFINED) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

PyObject *_set_type_static_impl(PyObject *type, int final) {
  PyTypeObject *pytype;
  if (!PyType_Check(type)) {
    PyErr_Format(PyExc_TypeError, "Expected a type object, not %.100s",
                 Py_TYPE(type)->tp_name);
    return NULL;
  }
  pytype = (PyTypeObject *)type;
  pytype->tp_flags |= Py_TPFLAGS_IS_STATICALLY_DEFINED;

  if (pytype->tp_cache != NULL) {
      /* If the v-table was inited because our base class was
       * already inited, it is no longer valid...  we need to include
       * statically defined methods (we'd be better off having custom
       * static class building which knows we're building a static type
       * from the get-go */
      Py_CLEAR(pytype->tp_cache);
      if (_PyClassLoader_EnsureVtable(pytype, 0) == NULL) {
          return NULL;
      }
  }

  if (final) {
    pytype->tp_flags &= ~Py_TPFLAGS_BASETYPE;
  }
  Py_INCREF(type);
  return type;
}

PyObject *set_type_static(PyObject *mod, PyObject *type) {
    return _set_type_static_impl(type, 0);
}

PyObject *set_type_static_final(PyObject *mod, PyObject *type) {
    return _set_type_static_impl(type, 1);
}

static PyObject *
_recreate_cm(PyObject *self) {
    Py_INCREF(self);
    return self;
}

PyObject *make_recreate_cm(PyObject *mod, PyObject *type) {
    static PyMethodDef def = {"_recreate_cm",
        (PyCFunction)&_recreate_cm,
        METH_NOARGS};

     if (!PyType_Check(type)) {
        PyErr_Format(PyExc_TypeError, "Expected a type object, not %.100s",
                    Py_TYPE(type)->tp_name);
         return NULL;
     }


    return PyDescr_NewMethod((PyTypeObject *)type, &def);
}

typedef struct {
    PyWeakReference weakref; /* base weak ref */
    PyObject *func;     /* function that's being wrapped */
    PyObject *ctxdec;   /* the instance of the ContextDecorator class */
    PyObject *enter;    /* borrowed ref to __enter__, valid on cache_version */
    PyObject *exit;     /* borrowed ref to __exit__, valid on cache_version */
    PyObject *recreate_cm; /* borrowed ref to recreate_cm, valid on recreate_cache_version */
    Py_ssize_t cache_version;
    Py_ssize_t recreate_cache_version;
    int is_coroutine;
} _Py_ContextManagerWrapper;

static PyObject *_return_none;

int
ctxmgrwrp_import_value(const char *module, const char *name, PyObject **dest) {
    PyObject *mod = PyImport_ImportModule(module);
    if (mod == NULL) {
        return -1;
    }
    if (*dest == NULL) {
        PyObject *value = PyObject_GetAttrString(mod, name);
        if (value == NULL) {
            return -1;
        }
        *dest = value;
    }
    Py_DECREF(mod);
    return 0;
}


static PyObject *
ctxmgrwrp_exit(int is_coroutine, PyObject *ctxmgr,
               PyObject *result, PyObject *exit)
{
    if (result == NULL) {
        // exception
        PyObject *ret;
        PyObject *exc, *val, *tb;
        PyErr_Fetch(&exc, &val, &tb);
        if (tb == NULL) {
            tb = Py_None;
            Py_INCREF(tb);
        }

        if (ctxmgr != NULL) {
            assert(Py_TYPE(exit)->tp_flags & Py_TPFLAGS_METHOD_DESCRIPTOR);
            PyObject* stack[] = {(PyObject *)ctxmgr, exc, val, tb};
            ret =  _PyObject_Vectorcall(exit, stack, 4 | _Py_VECTORCALL_INVOKED_METHOD, NULL);
        } else {
            PyObject* stack[] = {exc, val, tb};
            ret =  _PyObject_Vectorcall(exit, stack, 3 | _Py_VECTORCALL_INVOKED_METHOD, NULL);
        }
        if (ret == NULL) {
            Py_DECREF(exc);
            Py_DECREF(val);
            Py_DECREF(tb);
            return NULL;
        }

        int err = PyObject_IsTrue(ret);
        Py_DECREF(ret);
        if (!err) {
            PyErr_Restore(exc, val, tb);
            goto error;
        }

        Py_DECREF(exc);
        Py_DECREF(val);
        Py_DECREF(tb);
        if (err < 0) {
            goto error;
        }

        if (is_coroutine) {
            /* The co-routine needs to yield None instead of raising the exception.  We
             * need to actually produce a co-routine which is going to return None to
             * do that, so we have a helper function which does just that. */
            if (_return_none == NULL &&
                ctxmgrwrp_import_value("__static__", "_return_none", &_return_none)) {
                return NULL;
            }

            PyObject *call_none = _PyObject_CallNoArg(_return_none);
            if (call_none == NULL) {
                return NULL;
            }
            return call_none;
        }
        Py_RETURN_NONE;
    } else {
        PyObject *ret;
        if (ctxmgr != NULL) {
            /* we picked up a method like object and have self for it */
            assert(Py_TYPE(exit)->tp_flags & Py_TPFLAGS_METHOD_DESCRIPTOR);
            PyObject *stack[] = {(PyObject *) ctxmgr, Py_None, Py_None, Py_None};
            ret = _PyObject_Vectorcall(exit, stack, 4 | _Py_VECTORCALL_INVOKED_METHOD, NULL);
        } else {
            PyObject *stack[] = {Py_None, Py_None, Py_None};
            ret = _PyObject_Vectorcall(exit, stack, 3 | _Py_VECTORCALL_INVOKED_METHOD, NULL);
        }
        if (ret == NULL) {
            goto error;
        }
        Py_DECREF(ret);
    }

    return result;
error:
    Py_XDECREF(result);
    return NULL;
}

static PyObject *
ctxmgrwrp_cb(_PyClassLoader_Awaitable *awaitable, PyObject *result)
{
    /* In the error case our awaitable is done, and if we return a value
     * it'll turn into the returned value, so we don't want to pass iscoroutine
     * because we don't need a wrapper object. */
    if (awaitable->onsend != NULL) {
        /* Send has never happened, so we never called __enter__, so there's
         * no __exit__ to call. */
         return NULL;
    }
    return ctxmgrwrp_exit(result != NULL, NULL, result, awaitable->state);
}

extern int _PyObject_GetMethod(PyObject *, PyObject *, PyObject **);

static PyObject *
get_descr(PyObject *obj, PyObject *self)
{
    descrgetfunc f = Py_TYPE(obj)->tp_descr_get;
    if (f != NULL) {
        return f(obj, self, (PyObject *)Py_TYPE(self));
    }
    Py_INCREF(obj);
    return obj;
}

static PyObject *
call_with_self(PyThreadState *tstate, PyObject *func, PyObject *self)
{
    if (Py_TYPE(func)->tp_flags & Py_TPFLAGS_METHOD_DESCRIPTOR) {
        PyObject *args[1] = { self };
        return _PyObject_VectorcallTstate(tstate, func, args, 1|_Py_VECTORCALL_INVOKED_METHOD, NULL);
    } else {
        func = get_descr(func, self);
        if (func == NULL) {
            return NULL;
        }
        PyObject *ret = _PyObject_VectorcallTstate(tstate, func, NULL, 0|_Py_VECTORCALL_INVOKED_METHOD, NULL);
        Py_DECREF(func);
        return ret;
    }
}

static PyObject *
ctxmgrwrp_enter(_Py_ContextManagerWrapper *self, PyObject **ctxmgr)
{
    _Py_IDENTIFIER(__exit__);
    _Py_IDENTIFIER(__enter__);
    _Py_IDENTIFIER(_recreate_cm);

    PyThreadState *tstate = _PyThreadState_GET();

    if (self->recreate_cache_version != Py_TYPE(self->ctxdec)->tp_version_tag) {
        self->recreate_cm = _PyType_LookupId(Py_TYPE(self->ctxdec), &PyId__recreate_cm);
        if (self->recreate_cm == NULL) {
            PyErr_Format(PyExc_TypeError, "failed to resolve _recreate_cm on %s",
                        Py_TYPE(self->ctxdec)->tp_name);
            return NULL;
        }

        self->recreate_cache_version = Py_TYPE(self->ctxdec)->tp_version_tag;
    }

    PyObject *ctx_mgr = call_with_self(tstate, self->recreate_cm, self->ctxdec);
    if (ctx_mgr == NULL) {
        return NULL;
    }

    if (self->cache_version != Py_TYPE(ctx_mgr)->tp_version_tag) {
        /* we probably get the same type back from _recreate_cm over and
         * over again, so we cache the lookups for enter and exit */
        self->enter = _PyType_LookupId(Py_TYPE(ctx_mgr), &PyId___enter__);
        self->exit = _PyType_LookupId(Py_TYPE(ctx_mgr), &PyId___exit__);
        if (self->enter == NULL || self->exit == NULL) {
            Py_DECREF(ctx_mgr);
            PyErr_Format(PyExc_TypeError, "failed to resolve context manager on %s",
                        Py_TYPE(ctx_mgr)->tp_name);
            return NULL;
        }

        self->cache_version = Py_TYPE(ctx_mgr)->tp_version_tag;
    }

    PyObject *enter = self->enter;
    PyObject *exit = self->exit;

    Py_INCREF(enter);
    if (!(Py_TYPE(exit)->tp_flags & Py_TPFLAGS_METHOD_DESCRIPTOR)) {
        /* Descriptor protocol for exit needs to run before we call
         * user code */
        exit = get_descr(exit, ctx_mgr);
        Py_CLEAR(ctx_mgr);
        if (exit == NULL) {
            return NULL;
        }
    } else {
        Py_INCREF(exit);
    }

    PyObject *enter_res = call_with_self(tstate, enter, ctx_mgr);
    Py_DECREF(enter);

    if (enter_res == NULL) {
        goto error;
    }
    Py_DECREF(enter_res);

    *ctxmgr = ctx_mgr;
    return exit;
error:
    Py_DECREF(ctx_mgr);
    return NULL;
}

static int
ctxmgrwrp_first_send(_PyClassLoader_Awaitable *self) {
    /* Handles calling __enter__ on the first step of the co-routine when
     * we're not eagerly evaluated. We'll swap our state over to the exit
     * function from the _Py_ContextManagerWrapper once we're successful */
    _Py_ContextManagerWrapper *ctxmgrwrp = (_Py_ContextManagerWrapper *)self->state;
    PyObject *ctx_mgr;
    PyObject *exit = ctxmgrwrp_enter(ctxmgrwrp, &ctx_mgr);
    Py_DECREF(ctxmgrwrp);
    if (exit == NULL) {
        return -1;
    }
    if (ctx_mgr != NULL) {
        PyObject *bound_exit = get_descr(exit, ctx_mgr);
        if (bound_exit == NULL) {
            return -1;
        }
        Py_DECREF(exit);
        Py_DECREF(ctx_mgr);
        exit = bound_exit;
    }
    self->state = exit;
    return 0;
}

static PyObject *
ctxmgrwrp_make_awaitable(_Py_ContextManagerWrapper *ctxmgrwrp, PyObject *ctx_mgr,
                         PyObject *exit, PyObject *res, int eager)
{
    /* We won't have exit yet if we're not eagerly evaluated, and haven't called
     * __enter__ yet.  In that case we'll setup ctxmgrwrp_first_send to run on
     * the first iteration (with the wrapper as our state)) and then restore the
     * awaitable wrapper to our normal state of having exit as the state after
     * we've called __enter__ */
    if (ctx_mgr != NULL && exit != NULL) {
        PyObject *bound_exit = get_descr(exit, ctx_mgr);
        if (bound_exit == NULL) {
            return NULL;
        }
        Py_DECREF(exit);
        Py_DECREF(ctx_mgr);
        exit = bound_exit;
    }
    res = _PyClassLoader_NewAwaitableWrapper(res,
                                             eager,
                                             exit == NULL ? (PyObject *)ctxmgrwrp : exit,
                                             ctxmgrwrp_cb,
                                             exit == NULL ? ctxmgrwrp_first_send : NULL);
    Py_XDECREF(exit);
    return res;
}

PyTypeObject _PyContextDecoratorWrapper_Type;

static PyObject *
ctxmgrwrp_vectorcall(PyFunctionObject *func, PyObject *const *args,
                     Py_ssize_t nargsf, PyObject *kwargs)
{
    PyWeakReference *wr = (PyWeakReference *)func->func_weakreflist;
    while (wr != NULL && Py_TYPE(wr) != &_PyContextDecoratorWrapper_Type) {
        wr = wr->wr_next;
    }
    if (wr == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "missing weakref");
        return NULL;
    }
    _Py_ContextManagerWrapper *self = (_Py_ContextManagerWrapper *)wr;

    PyObject *ctx_mgr;
    PyObject *exit = NULL;

    /* If this is a co-routine, and we're not being eagerly evaluated, we cannot
     * start calling __enter__ just yet.  We'll delay that until the first step
     * of the coroutine.  Otherwise we're not a co-routine or we're eagerly
     * awaited in which case we'll call __enter__ now and capture __exit__
     * before any possible side effects to match the normal eval loop */
    if (!self->is_coroutine || nargsf & _Py_AWAITED_CALL_MARKER) {
        exit = ctxmgrwrp_enter(self, &ctx_mgr);
        if (exit == NULL) {
            return NULL;
        }
    }

    /* Call the wrapped function */
    PyObject *res = _PyObject_Vectorcall(self->func, args, nargsf, kwargs);
    if (self->is_coroutine && res != NULL) {
        /* If it's a co-routine either pass up the eagerly awaited value or
         * pass out a wrapping awaitable */
        int eager = _PyWaitHandle_CheckExact(res);
        if (eager) {
            PyWaitHandleObject *handle = (PyWaitHandleObject *)res;
            if (handle->wh_waiter == NULL) {
                assert(nargsf & _Py_AWAITED_CALL_MARKER && exit != NULL);
                res = ctxmgrwrp_exit(1, ctx_mgr, res, exit);
                Py_DECREF(exit);
                Py_XDECREF(ctx_mgr);
                if (res == NULL) {
                    _PyWaitHandle_Release((PyObject *)handle);
                }
                return res;
            }
        }
        return ctxmgrwrp_make_awaitable(self, ctx_mgr, exit, res, eager);
    }

    if (exit == NULL) {
        assert(self->is_coroutine && res == NULL);
        /* We must have failed producing the coroutine object for the
         * wrapped function, we haven't called __enter__, just report
         * out the error from creating the co-routine */
        return NULL;
    }

    /* Call __exit__ */
    res = ctxmgrwrp_exit(self->is_coroutine, ctx_mgr, res, exit);
    Py_XDECREF(ctx_mgr);
    Py_DECREF(exit);
    return res;
}

static int
ctxmgrwrp_traverse(_Py_ContextManagerWrapper *self, visitproc visit, void *arg)
{
    _PyWeakref_RefType.tp_traverse((PyObject *)self, visit, arg);
    Py_VISIT(self->ctxdec);
    return 0;
}

static int
ctxmgrwrp_clear(_Py_ContextManagerWrapper *self)
{
    _PyWeakref_RefType.tp_clear((PyObject *)self);
    Py_CLEAR(self->ctxdec);
    return 0;
}

static void
ctxmgrwrp_dealloc(_Py_ContextManagerWrapper *self)
{
    ctxmgrwrp_clear(self);
    _PyWeakref_RefType.tp_dealloc((PyObject *)self);
}

PyTypeObject _PyContextDecoratorWrapper_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0) "context_decorator_wrapper",
    sizeof(_Py_ContextManagerWrapper),
    .tp_base = &_PyWeakref_RefType,
    .tp_dealloc = (destructor)ctxmgrwrp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)ctxmgrwrp_traverse,
    .tp_clear = (inquiry)ctxmgrwrp_clear,
};

static PyObject *
weakref_callback_impl(PyObject *self, _Py_ContextManagerWrapper *weakref)
{
    /* the weakref provides a callback when the object it's tracking
       is freed.  The only thing holding onto this weakref is the
       function object we're tracking, so we rely upon this callback
       to free the weakref / context mgr wrapper. */
    Py_DECREF(weakref);

    Py_RETURN_NONE;
}

static PyMethodDef _WeakrefCallback = {
    "weakref_callback", (PyCFunction)weakref_callback_impl, METH_O, NULL};


static PyObject *weakref_callback;

PyObject *make_context_decorator_wrapper(PyObject *mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError, "expected 3 arguments: context decorator, wrapper func, and original func");
        return NULL;
    } else if (PyType_Ready(&_PyContextDecoratorWrapper_Type)) {
        return NULL;
    } else if (!PyFunction_Check(args[1])) {
        PyErr_SetString(PyExc_TypeError, "expected function for argument 2");
        return NULL;
    }

    PyFunctionObject *wrapper_func = (PyFunctionObject *)args[1];
    PyObject *wrapped_func = args[2];

    if (weakref_callback == NULL) {
        weakref_callback = PyCFunction_New(&_WeakrefCallback, NULL);
        if (weakref_callback == NULL) {
            return NULL;
        }
    }

    PyObject *wrargs = PyTuple_New(2);
    if (wrargs == NULL) {
        return NULL;
    }

    PyTuple_SET_ITEM(wrargs, 0, (PyObject *)wrapper_func);
    Py_INCREF(wrapper_func);
    PyTuple_SET_ITEM(wrargs, 1, weakref_callback);
    Py_INCREF(weakref_callback);

    _Py_ContextManagerWrapper *ctxmgr_wrapper = (_Py_ContextManagerWrapper *)_PyWeakref_RefType.tp_new(
        &_PyContextDecoratorWrapper_Type, wrargs, NULL);
    Py_DECREF(wrargs);

    if (ctxmgr_wrapper == NULL) {
        return NULL;
    }

    ctxmgr_wrapper->recreate_cache_version = -1;
    ctxmgr_wrapper->cache_version = -1;
    ctxmgr_wrapper->enter = ctxmgr_wrapper->exit = ctxmgr_wrapper->recreate_cm = NULL;
    ctxmgr_wrapper->ctxdec = args[0];
    Py_INCREF(args[0]);
    ctxmgr_wrapper->func = wrapped_func; /* borrowed, the weak ref will live as long as the function */
    ctxmgr_wrapper->is_coroutine = ((PyCodeObject *)wrapper_func->func_code)->co_flags & CO_COROUTINE;

    wrapper_func->func_weakreflist = (PyObject *)ctxmgr_wrapper;
    wrapper_func->vectorcall = (vectorcallfunc)ctxmgrwrp_vectorcall;

    Py_INCREF(wrapper_func);
    return (PyObject *)wrapper_func;
}


#define VECTOR_APPEND(size, sig_type, append)                                           \
    int vector_append_##size(PyObject *self, size##_t value) {                          \
        return append(self, value);                                                     \
    }                                                                                   \
                                                                                        \
    _Py_TYPED_SIGNATURE(vector_append_##size, _Py_SIG_ERROR, &sig_type, NULL);          \
                                                                                        \
    PyMethodDef md_vector_append_##size = {                                             \
        "append",                                                                       \
        (PyCFunction)&vector_append_##size##_def,                                       \
        METH_TYPED,                                                                     \
        "append(value: " #size ")"                                                      \
    };                                                                                  \

VECTOR_APPEND(int8, _Py_Sig_INT8, _PyArray_AppendSigned)
VECTOR_APPEND(int16, _Py_Sig_INT16, _PyArray_AppendSigned)
VECTOR_APPEND(int32, _Py_Sig_INT32, _PyArray_AppendSigned)
VECTOR_APPEND(int64, _Py_Sig_INT64, _PyArray_AppendSigned)
VECTOR_APPEND(uint8, _Py_Sig_INT8, _PyArray_AppendUnsigned)
VECTOR_APPEND(uint16, _Py_Sig_INT16, _PyArray_AppendUnsigned)
VECTOR_APPEND(uint32, _Py_Sig_INT32, _PyArray_AppendUnsigned)
VECTOR_APPEND(uint64, _Py_Sig_INT64, _PyArray_AppendUnsigned)

PyObject *specialize_function(PyObject *m, PyObject *const *args, Py_ssize_t nargs) {
    PyObject *type;
    PyObject *name;
    PyObject *params;
    if (!_PyArg_ParseStack(args, nargs, "O!UO!", &PyType_Type, &type, &name, &PyTuple_Type, &params)) {
        return NULL;
    }

    if (PyUnicode_CompareWithASCIIString(name, "Vector.append") == 0) {
        if (PyTuple_Size(params) != 1) {
            PyErr_SetString(PyExc_TypeError, "expected single type argument for Vector");
            return NULL;
        }

        switch (_PyClassLoader_GetTypeCode((PyTypeObject *)PyTuple_GET_ITEM(params, 0))) {
            case TYPED_INT8:
               return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_int8);
            case TYPED_INT16:
                return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_int16);
            case TYPED_INT32:
                return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_int32);
            case TYPED_INT64:
                return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_int64);
            case TYPED_UINT8:
                return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_uint8);
            case TYPED_UINT16:
                return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_uint16);
            case TYPED_UINT32:
                return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_uint32);
            case TYPED_UINT64:
                return PyDescr_NewMethod((PyTypeObject *)type, &md_vector_append_uint64);
            default:
                PyErr_SetString(PyExc_TypeError, "unsupported primitive array type");
                return NULL;
        }
    }

    PyErr_SetString(PyExc_TypeError, "unknown runtime helper");
    return NULL;
}

static int64_t
static_rand(PyObject *self)
{
    return rand();
}

_Py_TYPED_SIGNATURE(static_rand, _Py_SIG_INT32, NULL);


static int64_t posix_clock_gettime_ns(PyObject* mod)
{
    struct timespec result;
    int64_t ret;

    clock_gettime(CLOCK_MONOTONIC, &result);
    ret = result.tv_sec * 1e9 + result.tv_nsec;
    return ret;
}

_Py_TYPED_SIGNATURE(posix_clock_gettime_ns, _Py_SIG_INT64, NULL);

static Py_ssize_t
static_property_missing_fget(PyObject *mod, PyObject *self)
{
    PyErr_SetString(PyExc_AttributeError, "unreadable attribute");
    return -1;
}

_Py_TYPED_SIGNATURE(static_property_missing_fget, _Py_SIG_ERROR, &_Py_Sig_Object, NULL);

static Py_ssize_t
static_property_missing_fset(PyObject *mod, PyObject *self, PyObject *val)
{
    PyErr_SetString(PyExc_AttributeError, "can't set attribute");
    return -1;
}

_Py_TYPED_SIGNATURE(static_property_missing_fset, _Py_SIG_ERROR, &_Py_Sig_Object, &_Py_Sig_Object, NULL);


/*
    Static Python compiles cached properties into something like this:
        class C:
            __slots__ = ("x")

            def _x_impl(self): ...

            C.x = cached_property(C._x_impl, C.x)
            del C._x_impl

    The last two lines result in a STORE_ATTR + DELETE_ATTR. However, both those
    opcodes result in us creating a v-table on the C class. That's not correct, because
    the v-table should be created only _after_ `C.x` is assigned (and the impl deleted).

    This function does the job, without going through the v-table creation.
*/
static PyObject *
setup_cached_property_on_type(PyObject *Py_UNUSED(module), PyObject **args, Py_ssize_t nargs)
{
    if (nargs != 4) {
        PyErr_SetString(PyExc_TypeError, "Expected 4 arguments");
        return NULL;
    }
    PyObject *typ = args[0];
    if (!PyType_Check(typ)) {
        PyErr_SetString(PyExc_TypeError, "Expected a type object as 1st argument");
        return NULL;
    }
    PyObject *property = args[1];
    PyObject *name = args[2];
    if (!PyUnicode_Check(name)) {
        PyErr_SetString(PyExc_TypeError, "Expected str as 3rd argument (name of the cached property)");
        return NULL;
    }
    PyObject *impl_name = args[3];
    if (!PyUnicode_Check(impl_name)) {
        PyErr_SetString(PyExc_TypeError, "Expected str as 4th argument (name of the implementation slot)");
        return NULL;
    }

    // First setup the cached_property
    int res;
    res = _PyObject_GenericSetAttrWithDict(typ, name, property, NULL);
    if (res != 0) {
        return NULL;
    }

    // Next clear the backing slot
    res = _PyObject_GenericSetAttrWithDict(typ, impl_name, NULL, NULL);
    if (res != 0) {
        return NULL;
    }

    PyType_Modified((PyTypeObject*)typ);
    Py_RETURN_NONE;
}

static PyMethodDef static_methods[] = {
    {"set_type_code", (PyCFunction)(void(*)(void))set_type_code, METH_FASTCALL, ""},
    {"specialize_function", (PyCFunction)(void(*)(void))specialize_function, METH_FASTCALL, ""},
    {"rand", (PyCFunction)&static_rand_def, METH_TYPED, ""},
    {"is_type_static", (PyCFunction)(void(*)(void))is_type_static, METH_O, ""},
    {"set_type_static", (PyCFunction)(void(*)(void))set_type_static, METH_O, ""},
    {"set_type_static_final", (PyCFunction)(void(*)(void))set_type_static_final, METH_O, ""},
    {"make_recreate_cm", (PyCFunction)(void(*)(void))make_recreate_cm, METH_O, ""},
    {"posix_clock_gettime_ns", (PyCFunction)&posix_clock_gettime_ns_def, METH_TYPED,
     "Returns time in nanoseconds as an int64. Note: Does no error checks at all."},
    {"_property_missing_fget", (PyCFunction)&static_property_missing_fget_def, METH_TYPED, ""},
    {"_property_missing_fset", (PyCFunction)&static_property_missing_fset_def, METH_TYPED, ""},
    {"make_context_decorator_wrapper", (PyCFunction)(void(*)(void))make_context_decorator_wrapper, METH_FASTCALL, ""},
    {"_setup_cached_property_on_type", (PyCFunction)setup_cached_property_on_type, METH_FASTCALL, ""},
    {}
};

static struct PyModuleDef _staticmodule = {PyModuleDef_HEAD_INIT,
                                           "_static",
                                           _static__doc__,
                                           0,
                                           static_methods,
                                           _static_slots,
                                           NULL,
                                           NULL,
                                           NULL};

PyMODINIT_FUNC
PyInit__static(void)
{

    return PyModuleDef_Init(&_staticmodule);
}
