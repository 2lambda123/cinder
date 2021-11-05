/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */

#include "Python.h"
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
