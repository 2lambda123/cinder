/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#include "Python.h"

#include "internal/pycore_shadow_frame.h"
#include "frameobject.h"

#include "Jit/pyjit.h"

PyAPI_FUNC(void) _PyShadow_ClearCache(PyObject *co);

extern int _PyShadow_PolymorphicCacheEnabled;
extern int _Py_SkipFinalCleanup;
extern int _Py_SetShortcutTypeCall;

/* facebook begin */
static PyObject *
cinder_setknobs(PyObject *self, PyObject *o)
{
    if (!PyDict_CheckExact(o)) {
        PyErr_SetString(PyExc_ValueError, "expected dictionary of knobs");
        return NULL;
    }

    PyObject* shadowcode = PyDict_GetItemString(o, "shadowcode");
    if (shadowcode != NULL) {
        int enabled = PyObject_IsTrue(shadowcode);
        _PyEval_ShadowByteCodeEnabled = enabled != -1 && enabled;
    }

    PyObject* lazy_imports = PyDict_GetItemString(o, "lazyimports");
    if (lazy_imports != NULL) {
        int enabled = PyObject_IsTrue(lazy_imports);
        _PyEval_LazyImportsEnabled = enabled != -1 && enabled;
    }

    PyObject *genfreelist = PyDict_GetItemString(o, "genfreelist");
    if (genfreelist != NULL) {
        int enabled = PyObject_IsTrue(genfreelist);
        _PyGen_FreeListEnabled = enabled != -1 && enabled;
        if (!enabled) {
            _PyGen_ClearFreeList();
        }
    }

    PyObject *polymorphic = PyDict_GetItemString(o, "polymorphiccache");
    if (polymorphic != NULL) {
        int enabled = PyObject_IsTrue(polymorphic);
        _PyShadow_PolymorphicCacheEnabled = enabled != -1 && enabled;
    }

    PyObject *skip_final_cleanup = PyDict_GetItemString(o, "skipfinalcleanup");
    if (skip_final_cleanup) {
        int skip_cleanup = PyObject_IsTrue(skip_final_cleanup);
        _Py_SkipFinalCleanup = skip_cleanup != -1 && skip_cleanup;
    }
    PyObject *set_shortcut_typecall = PyDict_GetItemString(o, "setshortcuttypecall");
    if (set_shortcut_typecall) {
        int ok = PyObject_IsTrue(set_shortcut_typecall);
        _Py_SetShortcutTypeCall = ok != -1 && ok;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(setknobs_doc,
"setknobs(knobs)\n\
\n\
Sets the currently enabled knobs.  Knobs are provided as a dictionary of\n\
names and a value indicating if they are enabled.\n\
\n\
See cinder.getknobs() for a list of recognized knobs.");

static PyObject *
cinder_getknobs(PyObject *self, PyObject *args)
{
    PyObject* res = PyDict_New();
    if (res == NULL) {
        return NULL;
    }

    int err = PyDict_SetItemString(res, "shadowcode",
                         _PyEval_ShadowByteCodeEnabled ? Py_True : Py_False);
    if (err == -1)
        return NULL;

    err = PyDict_SetItemString(res, "lazyimports",
                         _PyEval_LazyImportsEnabled ? Py_True : Py_False);
    if (err == -1)
        return NULL;

    err = PyDict_SetItemString(
        res, "genfreelist", _PyGen_FreeListEnabled ? Py_True : Py_False);
    if (err == -1) {
        return NULL;
    }

    err = PyDict_SetItemString(res,
                               "skipfinalcleanup",
                               _Py_SkipFinalCleanup ? Py_True
                                                    : Py_False);

    if (err == -1) {
        return NULL;
    }

    err = PyDict_SetItemString(res,
                               "polymorphiccache",
                               _PyShadow_PolymorphicCacheEnabled ? Py_True
                                                                 : Py_False);
    if (err == -1) {
        return NULL;
    }

    return res;
}


PyDoc_STRVAR(getknobs_doc,
"getcinderknobs()\n\
\n\
Gets the available knobs and their current status.");
/* facebook end */


static PyObject *
cinder_freeze_type(PyObject *self, PyObject *o)
{
    if (!PyType_Check(o)) {
        PyErr_SetString(
            PyExc_TypeError,
            "freeze_type requires a type");
        return NULL;
    }
    ((PyTypeObject*)o)->tp_flags |= Py_TPFLAGS_FROZEN;
    Py_INCREF(o);
    return o;
}


PyDoc_STRVAR(freeze_type_doc,
"freeze_type(t)\n\
\n\
Marks a type as being frozen and disallows any future mutations to it."
);

static PyObject *
cinder_warn_on_inst_dict(PyObject *self, PyObject *o)
{
    if (!PyType_Check(o)) {
        PyErr_SetString(
            PyExc_TypeError,
            "warn_on_inst_dict requires a type");
        return NULL;
    } else if (((PyTypeObject *)o)->tp_flags & Py_TPFLAGS_FROZEN) {
        PyErr_SetString(
            PyExc_TypeError,
            "can't call warn_on_inst_dict on a frozen type");
        return NULL;
    }
    ((PyTypeObject *)o)->tp_flags |= Py_TPFLAGS_WARN_ON_SETATTR;
    Py_INCREF(o);
    return o;
}


PyDoc_STRVAR(cinder_warn_on_inst_dict_doc,
"warn_on_inst_dict(t)\n\
\n\
Causes a warning to be emitted when a type dictionary is created."
);

static PyObject *
cinder_set_warn_handler(PyObject *self, PyObject *o)
{
    Py_XDECREF(_PyErr_CinderWarnHandler);
    if (o == Py_None) {
        _PyErr_CinderWarnHandler = NULL;
    } else {
        _PyErr_CinderWarnHandler = o;
        Py_INCREF(_PyErr_CinderWarnHandler);
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(cinder_set_warn_handler_doc,
"set_warn_handler(cb)\n\
\n\
Sets a callback that receives Cinder specific warnings.\
\
Callback should be a callable that accepts:\
\
(message, *args)"
);

static PyObject *
cinder_get_warn_handler(PyObject *self, PyObject *args)
{
    if (_PyErr_CinderWarnHandler != NULL) {
        Py_INCREF(_PyErr_CinderWarnHandler);
        return _PyErr_CinderWarnHandler;
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(cinder_get_warn_handler_doc, "get_warn_handler()\n\
\n\
Gets the callback that receives Cinder specific warnings.");

static PyObject *
cinder_set_immutable_warn_handler(PyObject *self, PyObject *o)
{
    Py_XDECREF(_PyErr_ImmutableWarnHandler);
    if (o == Py_None) {
        _PyErr_ImmutableWarnHandler = NULL;
    } else {
        _PyErr_ImmutableWarnHandler = o;
        Py_INCREF(_PyErr_ImmutableWarnHandler);
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(cinder_set_immutable_warn_handler_doc,
"set_immutable_warn_handler(cb)\n\
\n\
Sets a callback that receives immutability specific warnings in Cinder.\
\
Callback should be a callable that accepts:\
\
(err_code, message, *args)"
);

static PyObject *
cinder_get_immutable_warn_handler(PyObject *self, PyObject *args)
{
    if (_PyErr_ImmutableWarnHandler != NULL) {
        Py_INCREF(_PyErr_ImmutableWarnHandler);
        return _PyErr_ImmutableWarnHandler;
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(cinder_get_immutable_warn_handler_doc, "get_immutable_warn_handler()\n\
\n\
Gets the callback that receives immutability specific warnings in Cinder.");


static PyObject *
cinder_raise_immutable_warning(PyObject *self, PyObject *args)
{
    int code;
    const char* msg;
    PyObject* value = NULL;
    if (!PyArg_ParseTuple(args, "is|O", &code, &msg, &value)) {
        return NULL;
    }
    if (_PyErr_IMMUTABLE_WARNING(code, msg, value) < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(cinder_raise_immutable_warning_doc, "raise_immutable_warn(code, msg, [arg])\n\
\n\
Manually raise a immutability warning.");

static PyObject *
cinder_flush_immutable_warnings(PyObject *self, PyObject *args)
{
    if (_PyErr_FlushImmutabilityWarningsBuffer() != 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(cinder_flush_immutable_warnings_doc, "flush_immutable_warnings()\n\
\n\
Manually flush the a immutability warning buffer.");

PyAPI_FUNC(void) _PyJIT_ClearDictCaches(void);

static PyObject *
clear_caches(PyObject *self, PyObject *obj)
{
    _PyJIT_ClearDictCaches();
    Py_RETURN_NONE;
}

static PyObject *
clear_shadow_cache(PyObject *self, PyObject *obj)
{
    _PyShadow_ClearCache(obj);
    Py_RETURN_NONE;
}


PyDoc_STRVAR(strict_module_patch_doc,
"strict_module_patch(mod, name, value)\n\
Patch a field in a strict module\n\
Requires patching to be enabled"
);
static PyObject * strict_module_patch(PyObject *self, PyObject *args)
{
    PyObject* mod;
    PyObject* name;
    PyObject* value;
    if (!PyArg_ParseTuple(args, "OUO", &mod, &name, &value)) {
        return NULL;
    }
    if (_Py_do_strictmodule_patch(mod, name, value) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(strict_module_patch_delete_doc,
"strict_module_patch_delete(mod, name)\n\
Delete a field in a strict module\n\
Requires patching to be enabled"
);
static PyObject * strict_module_patch_delete(PyObject *self, PyObject *args)
{
    PyObject* mod;
    PyObject* name;
    if (!PyArg_ParseTuple(args, "OU", &mod, &name)) {
        return NULL;
    }
    if (_Py_do_strictmodule_patch(mod, name, NULL) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(strict_module_patch_enabled_doc,
"strict_module_patch_enabled(mod)\n\
Gets whether patching is enabled on the strict module"
);
static PyObject * strict_module_patch_enabled(PyObject *self, PyObject *mod)
{

    if (((PyStrictModuleObject *) mod) -> global_setter != NULL) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}


PyAPI_FUNC(int) _PyClassLoader_ClearVtables(void);
PyAPI_FUNC(int) _PyClassLoader_ClearCache(void);

static PyObject *
clear_classloader_caches(PyObject *self, PyObject *obj)
{
    _PyClassLoader_ClearVtables();
    _PyClassLoader_ClearCache();
    Py_RETURN_NONE;
}

static PyObject *
get_qualname_of_code(PyObject *Py_UNUSED(module), PyObject *arg)
{
    if (!PyCode_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Expected code object");
        return NULL;
    }
    PyObject *qualname = ((PyCodeObject *)arg)->co_qualname;
    if (qualname != NULL) {
        Py_INCREF(qualname);
        return qualname;
    }
    Py_RETURN_NONE;
}

static PyObject *
set_qualname_of_code(PyObject *Py_UNUSED(module), PyObject **args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError, "Expected 2 arguments");
        return NULL;
    }
    PyObject *arg = args[0];
    if (!PyCode_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Expected code object as 1st argument");
        return NULL;
    }
    PyObject *qualname = args[1];
    if (qualname != Py_None) {
        if (!PyUnicode_Check(qualname)) {
            PyErr_SetString(PyExc_TypeError, "Expected str as 2nd argument");
            return NULL;
        }
        Py_XSETREF(((PyCodeObject *)arg)->co_qualname, qualname);
        Py_INCREF(qualname);
    }
    Py_RETURN_NONE;
}

static PyObject*
set_profile_interp(PyObject *self, PyObject *arg) {
    int is_true = PyObject_IsTrue(arg);
    if (is_true < 0) {
        return NULL;
    }

    PyThreadState* tstate = PyThreadState_Get();
    int old_flag = tstate->profile_interp;
    _PyThreadState_SetProfileInterp(tstate, is_true);

    if (old_flag) {
      Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject*
set_profile_interp_all(PyObject *self, PyObject *arg) {
    int is_true = PyObject_IsTrue(arg);
    if (is_true < 0) {
        return NULL;
    }
    g_profile_new_interp_threads = is_true;
    _PyThreadState_SetProfileInterpAll(is_true);

    Py_RETURN_NONE;
}

static PyObject*
set_profile_interp_period(PyObject *self, PyObject *arg) {
    if (!PyLong_Check(arg)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected int object, got %.200s",
                     Py_TYPE(arg)->tp_name);
        return NULL;
    }
    long val = PyLong_AsLong(arg);
    if (val == -1 && PyErr_Occurred()) {
        return NULL;
    }

    _PyRuntimeState_SetProfileInterpPeriod(val);
    Py_RETURN_NONE;
}

static PyObject*
get_and_clear_type_profiles(PyObject *self, PyObject *obj) {
    return _PyJIT_GetAndClearTypeProfiles();
}

static PyObject*
clear_type_profiles(PyObject *self, PyObject *obj) {
    _PyJIT_ClearTypeProfiles();
    Py_RETURN_NONE;
}

static PyObject*
get_frame_gen(PyObject *self, PyObject *frame) {
    if (!PyFrame_Check(frame)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected frame object, got %.200s",
                     Py_TYPE(frame)->tp_name);
        return NULL;
    }
    PyObject *gen = ((PyFrameObject *)frame)->f_gen;
    if (!gen) {
        Py_RETURN_NONE;
    }
    Py_INCREF(gen);
    return gen;
}

static PyObject*
get_coro_awaiter(PyObject *Py_UNUSED(self), PyObject *coro) {
    if (!PyCoro_CheckExact(coro)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected coroutine object, got %.200s",
                     Py_TYPE(coro)->tp_name);
        return NULL;
    }
    PyCoroObject *awaiter = ((PyCoroObject *)coro)->cr_awaiter;
    if (!awaiter) {
        Py_RETURN_NONE;
    }
    Py_INCREF(awaiter);
    return (PyObject *)awaiter;
}

static PyObject*
has_no_shadowing_instances(PyObject *self, PyObject *type) {
    if (!PyType_Check(type)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected type object, got %.200s",
                     Py_TYPE(type)->tp_name);
        return NULL;
    }
    if (PyType_HasFeature((PyTypeObject *) type, Py_TPFLAGS_NO_SHADOWING_INSTANCES)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject*
get_call_stack(PyObject *self, PyObject *args) {
    _PyShadowFrame *shadow_frame = PyThreadState_GET()->shadow_frame;
    PyObject *stack = PyList_New(0);
    if (stack == NULL) {
        return NULL;
    }
    while (shadow_frame != NULL) {
        PyCodeObject *code = _PyShadowFrame_GetCode(shadow_frame);
        if (PyList_Append(stack, (PyObject *) code) != 0) {
            Py_DECREF(stack);
            return NULL;
        }
        shadow_frame = shadow_frame->prev;
    }
    if (PyList_Reverse(stack) != 0) {
        Py_DECREF(stack);
        return NULL;
    }
    return stack;
}

static PyObject*
get_entire_call_stack_as_qualnames(PyObject *self, PyObject *Py_UNUSED(args)) {
  _PyShadowFrame *shadow_frame = PyThreadState_GET()->shadow_frame;
  _PyShadowFrame *last = NULL;
  _PyShadowFrame *awaiter_frame = NULL;
  PyObject *fqname;
  PyObject *stack = PyList_New(0);
  int did_fail;

  if (stack == NULL) {
    goto err;
  }

  while (shadow_frame != NULL) {
    fqname = _PyShadowFrame_GetFullyQualifiedName(shadow_frame);
    if (!fqname) {
      goto err;
    }

    did_fail = PyList_Append(stack, fqname);
    Py_DECREF(fqname);
    if (did_fail) {
      goto err;
    }

    last = shadow_frame;
    shadow_frame = shadow_frame->prev;

    // The awaiter stack (if it exists) should always get the preference
    awaiter_frame = _PyShadowFrame_GetAwaiterFrame(last);
    if (awaiter_frame != NULL) {
      shadow_frame = awaiter_frame;
    }
  }

  if (PyList_Reverse(stack) != 0) {
    goto err;
  }
  return stack;

err:
  Py_XDECREF(stack);
  return NULL;
}

static struct PyMethodDef cinder_module_methods[] = {
    {"setknobs", cinder_setknobs, METH_O, setknobs_doc},
    {"getknobs", cinder_getknobs, METH_NOARGS, getknobs_doc},
    {"freeze_type", cinder_freeze_type, METH_O, freeze_type_doc},
    {"warn_on_inst_dict",
     cinder_warn_on_inst_dict,
     METH_O,
     cinder_warn_on_inst_dict_doc},
    {"cinder_set_warn_handler",
     cinder_set_warn_handler,
     METH_O,
     cinder_set_warn_handler_doc},
    {"set_warn_handler",
     cinder_set_warn_handler,
     METH_O,
     cinder_set_warn_handler_doc},
    {"get_warn_handler",
     cinder_get_warn_handler,
     METH_NOARGS,
     cinder_get_warn_handler_doc},
    {"set_immutable_warn_handler",
     cinder_set_immutable_warn_handler,
     METH_O,
     cinder_set_immutable_warn_handler_doc},
    {"get_immutable_warn_handler",
     cinder_get_immutable_warn_handler,
     METH_NOARGS,
     cinder_get_immutable_warn_handler_doc},
    {"raise_immutable_warning",
     cinder_raise_immutable_warning,
     METH_VARARGS,
     cinder_raise_immutable_warning_doc},
    {"flush_immutable_warnings",
     cinder_flush_immutable_warnings,
     METH_NOARGS,
     cinder_flush_immutable_warnings_doc},
    {"clear_caches",
     clear_caches,
     METH_NOARGS,
     "Clears caches associated with the JIT.  This may have a negative effect "
     "on performance of existing JIT compiled code."},
    {"clear_shadow_cache", clear_shadow_cache, METH_O, ""},
    {"strict_module_patch",
     strict_module_patch,
     METH_VARARGS,
     strict_module_patch_doc},
    {"strict_module_patch_delete",
     strict_module_patch_delete,
     METH_VARARGS,
     strict_module_patch_delete_doc},
    {"strict_module_patch_enabled",
     strict_module_patch_enabled,
     METH_O,
     strict_module_patch_enabled_doc},
    {"clear_classloader_caches",
     clear_classloader_caches,
     METH_NOARGS,
     "Clears classloader caches and vtables on all accessible types. "
     "Will hurt perf; for test isolation where modules and types with "
     "identical names are dynamically created and destroyed."},
    {"_get_qualname",
     get_qualname_of_code,
     METH_O,
     "Returns qualified name stored in code object or None if codeobject was created manually"},
    {"_set_qualname",
     (PyCFunction)set_qualname_of_code,
     METH_FASTCALL,
     "Sets the value of qualified name in code object"},
    {"set_profile_interp",
     set_profile_interp,
     METH_O,
     "Enable or disable interpreter profiling for this thread. Returns whether or not profiling was enabled before the call."},
    {"set_profile_interp_all",
     set_profile_interp_all,
     METH_O,
     "Enable or disable interpreter profiling for all threads, including threads created after this function returns."},
    {"set_profile_interp_period",
     set_profile_interp_period,
     METH_O,
     "Set the period, in bytecode instructions, for interpreter profiling."},
    {"get_and_clear_type_profiles",
     get_and_clear_type_profiles,
     METH_NOARGS,
     "Get and clear accumulated interpreter type profiles."},
    {"clear_type_profiles",
     clear_type_profiles,
     METH_NOARGS,
     "Clear accumulated interpreter type profiles."},
    {"_get_frame_gen",
     get_frame_gen,
     METH_O,
     "Get the generator associated with the given frame, or None if one "
     "doesn't exist."},
    {"_get_coro_awaiter",
     get_coro_awaiter,
     METH_O,
     "Get the awaiter of the given coroutine, or None if one is not set."},
    {"_has_no_shadowing_instances",
     has_no_shadowing_instances,
     METH_O,
     "Return whether or not the given type has TP_FLAGS_NO_SHADOWING_INSTACES set."},
    {"_get_call_stack",
     get_call_stack,
     METH_NOARGS,
     "Return a list that contains the code object for each function on the call"
     " stack, top-most frame last."},
    {"_get_entire_call_stack_as_qualnames",
        get_entire_call_stack_as_qualnames,
        METH_NOARGS,
        "Return the current stack as a list of qualnames."},
    {NULL, NULL} /* sentinel */
};

PyDoc_STRVAR(doc_cinder, "Cinder specific methods and types");

static struct PyModuleDef cindermodule = {
    PyModuleDef_HEAD_INIT,
    "cinder",
    doc_cinder,
    -1,
    cinder_module_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit_cinder(void)
{
    PyObject *m;
    /* Create the module and add the functions */
    m = PyModule_Create(&cindermodule);
    if (m == NULL) {
        return NULL;
    }
    PyObject* data_version = PyLong_FromLong(1);
    if (data_version == NULL) {
        return NULL;
    }
    if (PyObject_SetAttrString(
            m, "STRUCTURED_DATA_VERSION", data_version) < 0) {
        return NULL;
    }


    if (PyType_Ready(&PyCachedProperty_Type) < 0) {
        return NULL;
    }
    if (PyType_Ready(&PyCachedPropertyWithDescr_Type) < 0) {
        return NULL;
    }
    if (PyType_Ready(&PyStrictModule_Type) < 0) {
        return NULL;
    }
    if (PyType_Ready(&PyAsyncCachedProperty_Type) < 0) {
        return NULL;
    }
    if (PyType_Ready(&PyAsyncCachedClassProperty_Type) < 0) {
        return NULL;
    }

    PyObject *cached_classproperty = PyType_FromSpec(&_PyCachedClassProperty_TypeSpec);
    if (cached_classproperty == NULL) {
        return NULL;
    }

#define ADDITEM(NAME, OBJECT) \
    if (PyObject_SetAttrString(m, NAME, (PyObject *)OBJECT) < 0) {      \
        Py_DECREF(cached_classproperty);                                \
        return NULL;                                                    \
    }

    ADDITEM("cached_classproperty", cached_classproperty);
    ADDITEM("cached_property", &PyCachedProperty_Type);
    ADDITEM("StrictModule", &PyStrictModule_Type);
    ADDITEM("async_cached_property", &PyAsyncCachedProperty_Type);
    ADDITEM("async_cached_classproperty", &PyAsyncCachedClassProperty_Type);
    Py_DECREF(cached_classproperty);

#undef ADDITEM

    return m;
}
