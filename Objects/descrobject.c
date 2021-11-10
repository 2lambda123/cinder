/* Descriptors -- a new, flexible way to describe attributes */

#include "Python.h"
#include "pycore_object.h"
#include "pycore_pystate.h"
#include "pycore_pyerrors.h"
#include "pycore_tupleobject.h"
#include "structmember.h" /* Why is this not included in Python.h? */
#include "classloader.h"

/*[clinic input]
class mappingproxy "mappingproxyobject *" "&PyDictProxy_Type"
class property "propertyobject *" "&PyProperty_Type"
class async_cached_property "PyAsyncCachedPropertyDescrObject *" "&PyAsyncCachedProperty_Type"
class async_cached_classproperty "PyAsyncCachedClassPropertyDescrObject *" "PyAsyncCachedClassProperty_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=6b7166b85bfc13b6]*/

static void
descr_dealloc(PyDescrObject *descr)
{
    _PyObject_GC_UNTRACK(descr);
    Py_XDECREF(descr->d_type);
    Py_XDECREF(descr->d_name);
    Py_XDECREF(descr->d_qualname);
    PyObject_GC_Del(descr);
}

static PyObject *
descr_name(PyDescrObject *descr)
{
    if (descr->d_name != NULL && PyUnicode_Check(descr->d_name))
        return descr->d_name;
    return NULL;
}

static PyObject *
descr_repr(PyDescrObject *descr, const char *format)
{
    PyObject *name = NULL;
    if (descr->d_name != NULL && PyUnicode_Check(descr->d_name))
        name = descr->d_name;

    return PyUnicode_FromFormat(format, name, "?", descr->d_type->tp_name);
}

static PyObject *
method_repr(PyMethodDescrObject *descr)
{
    return descr_repr((PyDescrObject *)descr,
                      "<method '%V' of '%s' objects>");
}

static PyObject *
member_repr(PyMemberDescrObject *descr)
{
    return descr_repr((PyDescrObject *)descr,
                      "<member '%V' of '%s' objects>");
}

static PyObject *
getset_repr(PyGetSetDescrObject *descr)
{
    return descr_repr((PyDescrObject *)descr,
                      "<attribute '%V' of '%s' objects>");
}

static PyObject *
wrapperdescr_repr(PyWrapperDescrObject *descr)
{
    return descr_repr((PyDescrObject *)descr,
                      "<slot wrapper '%V' of '%s' objects>");
}

static int
descr_check(PyDescrObject *descr, PyObject *obj, PyObject **pres)
{
    if (obj == NULL) {
        Py_INCREF(descr);
        *pres = (PyObject *)descr;
        return 1;
    }
    if (!PyObject_TypeCheck(obj, descr->d_type)) {
        PyErr_Format(PyExc_TypeError,
                     "descriptor '%V' for '%.100s' objects "
                     "doesn't apply to a '%.100s' object",
                     descr_name((PyDescrObject *)descr), "?",
                     descr->d_type->tp_name,
                     obj->ob_type->tp_name);
        *pres = NULL;
        return 1;
    }
    return 0;
}

static PyObject *
classmethod_get(PyMethodDescrObject *descr, PyObject *obj, PyObject *type)
{
    /* Ensure a valid type.  Class methods ignore obj. */
    if (type == NULL) {
        if (obj != NULL)
            type = (PyObject *)obj->ob_type;
        else {
            /* Wot - no type?! */
            PyErr_Format(PyExc_TypeError,
                         "descriptor '%V' for type '%.100s' "
                         "needs either an object or a type",
                         descr_name((PyDescrObject *)descr), "?",
                         PyDescr_TYPE(descr)->tp_name);
            return NULL;
        }
    }
    if (!PyType_Check(type)) {
        PyErr_Format(PyExc_TypeError,
                     "descriptor '%V' for type '%.100s' "
                     "needs a type, not a '%.100s' as arg 2",
                     descr_name((PyDescrObject *)descr), "?",
                     PyDescr_TYPE(descr)->tp_name,
                     type->ob_type->tp_name);
        return NULL;
    }
    if (!PyType_IsSubtype((PyTypeObject *)type, PyDescr_TYPE(descr))) {
        PyErr_Format(PyExc_TypeError,
                     "descriptor '%V' requires a subtype of '%.100s' "
                     "but received '%.100s'",
                     descr_name((PyDescrObject *)descr), "?",
                     PyDescr_TYPE(descr)->tp_name,
                     ((PyTypeObject *)type)->tp_name);
        return NULL;
    }
    return PyCFunction_NewEx(descr->d_method, type, NULL);
}

static PyObject *
method_get(PyMethodDescrObject *descr, PyObject *obj, PyObject *type)
{
    PyObject *res;

    if (descr_check((PyDescrObject *)descr, obj, &res))
        return res;
    return PyCFunction_NewEx(descr->d_method, obj, NULL);
}

static PyObject *
member_get(PyMemberDescrObject *descr, PyObject *obj, PyObject *type)
{
    PyObject *res;

    if (descr_check((PyDescrObject *)descr, obj, &res))
        return res;

    if (descr->d_member->flags & READ_RESTRICTED) {
        if (PySys_Audit("object.__getattr__", "Os",
            obj ? obj : Py_None, descr->d_member->name) < 0) {
            return NULL;
        }
    }

    return PyMember_GetOne((char *)obj, descr->d_member);
}

static PyObject *
getset_get(PyGetSetDescrObject *descr, PyObject *obj, PyObject *type)
{
    PyObject *res;

    if (descr_check((PyDescrObject *)descr, obj, &res))
        return res;
    if (descr->d_getset->get != NULL)
        return descr->d_getset->get(obj, descr->d_getset->closure);
    PyErr_Format(PyExc_AttributeError,
                 "attribute '%V' of '%.100s' objects is not readable",
                 descr_name((PyDescrObject *)descr), "?",
                 PyDescr_TYPE(descr)->tp_name);
    return NULL;
}

static PyObject *
wrapperdescr_get(PyWrapperDescrObject *descr, PyObject *obj, PyObject *type)
{
    PyObject *res;

    if (descr_check((PyDescrObject *)descr, obj, &res))
        return res;
    return PyWrapper_New((PyObject *)descr, obj);
}

static int
descr_setcheck(PyDescrObject *descr, PyObject *obj, PyObject *value,
               int *pres)
{
    assert(obj != NULL);
    if (!PyObject_TypeCheck(obj, descr->d_type)) {
        PyErr_Format(PyExc_TypeError,
                     "descriptor '%V' for '%.100s' objects "
                     "doesn't apply to a '%.100s' object",
                     descr_name(descr), "?",
                     descr->d_type->tp_name,
                     obj->ob_type->tp_name);
        *pres = -1;
        return 1;
    }
    return 0;
}

static int
member_set(PyMemberDescrObject *descr, PyObject *obj, PyObject *value)
{
    int res;

    if (descr_setcheck((PyDescrObject *)descr, obj, value, &res))
        return res;
    return PyMember_SetOne((char *)obj, descr->d_member, value);
}

static int
getset_set(PyGetSetDescrObject *descr, PyObject *obj, PyObject *value)
{
    int res;

    if (descr_setcheck((PyDescrObject *)descr, obj, value, &res))
        return res;
    if (descr->d_getset->set != NULL)
        return descr->d_getset->set(obj, value,
                                    descr->d_getset->closure);
    PyErr_Format(PyExc_AttributeError,
                 "attribute '%V' of '%.100s' objects is not writable",
                 descr_name((PyDescrObject *)descr), "?",
                 PyDescr_TYPE(descr)->tp_name);
    return -1;
}


/* Vectorcall functions for each of the PyMethodDescr calling conventions.
 *
 * First, common helpers
 */
static const char *
get_name(PyObject *func) {
    assert(PyObject_TypeCheck(func, &PyMethodDescr_Type));
    return ((PyMethodDescrObject *)func)->d_method->ml_name;
}

typedef void (*funcptr)(void);

static inline int
method_check_args(PyObject *func,
                  PyObject *const *args,
                  Py_ssize_t nargs,
                  size_t nargsf,
                  PyObject *kwnames)
{
    assert(!PyErr_Occurred());
    assert(PyObject_TypeCheck(func, &PyMethodDescr_Type));
    if (nargs < 1) {
        PyErr_Format(PyExc_TypeError,
                     "descriptor '%.200s' of '%.100s' "
                     "object needs an argument",
                     get_name(func), PyDescr_TYPE(func)->tp_name);
        return -1;
    }
    if (!(nargsf & (_Py_VECTORCALL_INVOKED_STATICALLY |
                    _Py_VECTORCALL_INVOKED_METHOD))) {
        PyObject *self = args[0];
        if (!_PyObject_RealIsSubclass((PyObject *)Py_TYPE(self),
                                      (PyObject *)PyDescr_TYPE(func))) {
            PyErr_Format(PyExc_TypeError,
                         "descriptor '%.200s' for '%.100s' objects "
                         "doesn't apply to a '%.100s' object",
                         get_name(func),
                         PyDescr_TYPE(func)->tp_name,
                         Py_TYPE(self)->tp_name);
            return -1;
        }
    }
    if (kwnames && PyTuple_GET_SIZE(kwnames)) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s() takes no keyword arguments", get_name(func));
        return -1;
    }
    return 0;
}

static inline funcptr
method_enter_call(PyObject *func)
{
    if (Py_EnterRecursiveCall(" while calling a Python object")) {
        return NULL;
    }
    return (funcptr)((PyMethodDescrObject *)func)->d_method->ml_meth;
}

/* Now the actual vectorcall functions */
static PyObject *
method_vectorcall_VARARGS(
    PyObject *func, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (method_check_args(func, args, nargs, nargsf, kwnames)) {
        return NULL;
    }
    PyObject *argstuple = _PyTuple_FromArray(args+1, nargs-1);
    if (argstuple == NULL) {
        return NULL;
    }
    PyCFunction meth = (PyCFunction)method_enter_call(func);
    if (meth == NULL) {
        Py_DECREF(argstuple);
        return NULL;
    }
    PyObject *result = meth(args[0], argstuple);
    Py_DECREF(argstuple);
    Py_LeaveRecursiveCall();
    return result;
}

static PyObject *
method_vectorcall_VARARGS_KEYWORDS(
    PyObject *func, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (method_check_args(func, args, nargs, nargsf, NULL)) {
        return NULL;
    }
    PyObject *argstuple = _PyTuple_FromArray(args+1, nargs-1);
    if (argstuple == NULL) {
        return NULL;
    }
    PyObject *result = NULL;
    /* Create a temporary dict for keyword arguments */
    PyObject *kwdict = NULL;
    if (kwnames != NULL && PyTuple_GET_SIZE(kwnames) > 0) {
        kwdict = _PyStack_AsDict(args + nargs, kwnames);
        if (kwdict == NULL) {
            goto exit;
        }
    }
    PyCFunctionWithKeywords meth = (PyCFunctionWithKeywords)
                                   method_enter_call(func);
    if (meth == NULL) {
        goto exit;
    }
    result = meth(args[0], argstuple, kwdict);
    Py_LeaveRecursiveCall();
exit:
    Py_DECREF(argstuple);
    Py_XDECREF(kwdict);
    return result;
}

static PyObject *
method_vectorcall_FASTCALL(
    PyObject *func, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (method_check_args(func, args, nargs, nargsf, kwnames)) {
        return NULL;
    }
    _PyCFunctionFast meth = (_PyCFunctionFast)
                            method_enter_call(func);
    if (meth == NULL) {
        return NULL;
    }
    PyObject *result = meth(args[0], args+1, nargs-1);
    Py_LeaveRecursiveCall();
    return result;
}

static PyObject *
method_vectorcall_FASTCALL_KEYWORDS(
    PyObject *func, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (method_check_args(func, args, nargs, nargsf, NULL)) {
        return NULL;
    }
    _PyCFunctionFastWithKeywords meth = (_PyCFunctionFastWithKeywords)
                                        method_enter_call(func);
    if (meth == NULL) {
        return NULL;
    }
    PyObject *result = meth(args[0], args+1, nargs-1, kwnames);
    Py_LeaveRecursiveCall();
    return result;
}

static PyObject *
method_vectorcall_NOARGS(
    PyObject *func, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (method_check_args(func, args, nargs, nargsf, kwnames)) {
        return NULL;
    }
    if (nargs != 1) {
        PyErr_Format(PyExc_TypeError,
            "%.200s() takes no arguments (%zd given)", get_name(func), nargs-1);
        return NULL;
    }
    PyCFunction meth = (PyCFunction)method_enter_call(func);
    if (meth == NULL) {
        return NULL;
    }
    PyObject *result = meth(args[0], NULL);
    Py_LeaveRecursiveCall();
    return result;
}

static PyObject *
method_vectorcall_O(
    PyObject *func, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (method_check_args(func, args, nargs, nargsf, kwnames)) {
        return NULL;
    }
    if (nargs != 2) {
        PyErr_Format(PyExc_TypeError,
            "%.200s() takes exactly one argument (%zd given)",
            get_name(func), nargs-1);
        return NULL;
    }
    PyCFunction meth = (PyCFunction)method_enter_call(func);
    if (meth == NULL) {
        return NULL;
    }
    PyObject *result = meth(args[0], args[1]);
    Py_LeaveRecursiveCall();
    return result;
}

typedef void *(*call_self_0)(PyObject *self);
typedef void *(*call_self_1)(PyObject *self, void *);
typedef void *(*call_self_2)(PyObject *self, void *, void *);

PyObject *
method_vectorcall_typed_0(PyObject *func,
                          PyObject *const *args,
                          size_t nargsf,
                          PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (method_check_args(func, args, nargs, nargsf, kwnames)) {
        return NULL;
    }
    if (nargs != 1) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s() takes exactly one argument (%zd given)",
                     get_name(func),
                     nargs - 1);
        return NULL;
    }

    _PyTypedMethodDef *def = (_PyTypedMethodDef *)method_enter_call(func);
    if (def == NULL) {
        return NULL;
    }

    PyObject *self = args[0];
    void *res = ((call_self_0)def->tmd_meth)(self);
    res = _PyClassLoader_ConvertRet(res, def->tmd_ret);

    Py_LeaveRecursiveCall();
    return (PyObject *)res;
}

#define CONV_ARGS(n)                                                          \
    void *final_args[n];                                                      \
    for (Py_ssize_t i = 0; i < n; i++) {                                      \
        final_args[i] =                                                       \
            _PyClassLoader_ConvertArg(self, def->tmd_sig[i], i + 1, nargsf, args, \
                                      &error);                                \
        if (error) {                                                          \
            if (!PyErr_Occurred()) {                                          \
                _PyClassLoader_ArgError(get_name(func), i + 1, i,           \
                                        def->tmd_sig[i],  self);              \
            }                                                                 \
            goto done;                                                        \
        }                                                                     \
    }

PyObject *
method_vectorcall_typed_1(PyObject *func,
                          PyObject *const *args,
                          size_t nargsf,
                          PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (!(nargsf & _Py_VECTORCALL_INVOKED_STATICALLY)) {
        if (method_check_args(func, args, nargs, nargsf, kwnames)) {
            return NULL;
        } else if (nargs > 2) {
            PyErr_Format(PyExc_TypeError,
                            "%.200s() takes at most 1 argument, got %zd",
                            get_name(func),
                            nargs - 1);
            return NULL;
        }
    }

    _PyTypedMethodDef *def = (_PyTypedMethodDef *)method_enter_call(func);
    if (def == NULL) {
        return NULL;
    }

    PyObject *self = args[0];
    int error = 0;
    void *res = NULL;
    CONV_ARGS(1);

    res = ((call_self_1)def->tmd_meth)(self, final_args[0]);
    res = _PyClassLoader_ConvertRet(res, def->tmd_ret);

done:
    Py_LeaveRecursiveCall();
    return (PyObject *)res;
}

PyObject *
method_vectorcall_typed_2(PyObject *func,
                          PyObject *const *args,
                          size_t nargsf,
                          PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);

    if (!(nargsf & _Py_VECTORCALL_INVOKED_STATICALLY)) {
        if (method_check_args(func, args, nargs, nargsf, kwnames)) {
            return NULL;
        } else if (nargs > 3) {
            PyErr_Format(PyExc_TypeError,
                        "%.200s() expected at most 2 arguments, got %zd",
                        get_name(func),
                        nargs - 1);
            return NULL;
        }
    }

    _PyTypedMethodDef *def = (_PyTypedMethodDef *)method_enter_call(func);
    if (def == NULL) {
        return NULL;
    }

    PyObject *self = args[0];
    int error = 0;
    void *res = NULL;
    CONV_ARGS(2);


    res = ((call_self_2)def->tmd_meth)(self, final_args[0], final_args[1]);
    res = _PyClassLoader_ConvertRet(res, def->tmd_ret);

done:
    Py_LeaveRecursiveCall();
    return (PyObject *)res;
}

static PyObject *
classmethoddescr_call(PyMethodDescrObject *descr, PyObject *args,
                      PyObject *kwds)
{
    Py_ssize_t argc;
    PyObject *self, *result;
    PyThreadState *tstate = PyThreadState_GET();

    /* Make sure that the first argument is acceptable as 'self' */
    assert(PyTuple_Check(args));
    argc = PyTuple_GET_SIZE(args);
    if (argc < 1) {
        _PyErr_Format(tstate, PyExc_TypeError,
                     "descriptor '%V' of '%.100s' "
                     "object needs an argument",
                     descr_name((PyDescrObject *)descr), "?",
                     PyDescr_TYPE(descr)->tp_name);
        return NULL;
    }
    self = PyTuple_GET_ITEM(args, 0);
    if (!PyType_Check(self)) {
        _PyErr_Format(tstate, PyExc_TypeError,
                     "descriptor '%V' requires a type "
                     "but received a '%.100s' instance",
                     descr_name((PyDescrObject *)descr), "?",
                     self->ob_type->tp_name);
        return NULL;
    }
    if (!PyType_IsSubtype((PyTypeObject *)self, PyDescr_TYPE(descr))) {
        _PyErr_Format(tstate, PyExc_TypeError,
                     "descriptor '%V' requires a subtype of '%.100s' "
                     "but received '%.100s'",
                     descr_name((PyDescrObject *)descr), "?",
                     PyDescr_TYPE(descr)->tp_name,
                     ((PyTypeObject*)self)->tp_name);
        return NULL;
    }

    result = _PyMethodDef_RawFastCallDict(descr->d_method, self,
                                          &_PyTuple_ITEMS(args)[1], argc - 1,
                                          kwds);
    result = _Py_CheckFunctionResultTstate(tstate, (PyObject *)descr, result, NULL);
    return result;
}

Py_LOCAL_INLINE(PyObject *)
wrapperdescr_raw_call(PyWrapperDescrObject *descr, PyObject *self,
                      PyObject *const* stack, Py_ssize_t nargs, PyObject *kwnames)
{
    wrapperfunc wrapper = descr->d_base->wrapper;

    if (descr->d_base->flags & PyWrapperFlag_KEYWORDS) {
        PyObject *args = _PyTuple_FromArrayNoTrack(stack, nargs);
        if (args == NULL) {
            return NULL;
        }
        PyObject *kwds = NULL;
        if (kwnames != NULL && PyTuple_GET_SIZE(kwnames) != 0) {
            kwds = _PyStack_AsDict(stack + nargs, kwnames);
            if (kwds == NULL) {
                Py_DECREF(args);
                return NULL;
            }
        }
        wrapperfunc_kwds wk = (wrapperfunc_kwds)(void(*)(void))wrapper;
        PyObject *res = (*wk)(self, args, descr->d_wrapped, kwds);
        Py_XDECREF(kwds);
        PyTupleDECREF_MAYBE_TRACK(args);
        return res;
    }

    if (kwnames != NULL && (!PyTuple_Check(kwnames) || PyTuple_GET_SIZE(kwnames) != 0)) {
        PyErr_Format(PyExc_TypeError,
                     "wrapper %s() takes no keyword arguments",
                     descr->d_base->name);
        return NULL;
    }
    return (*wrapper)(self, stack, nargs, descr->d_wrapped);
}

static PyObject *
wrapperdescr_vectorcall(PyWrapperDescrObject *descr,
    PyObject *const* stack, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t argc = PyVectorcall_NARGS(nargsf);
    PyObject *self, *result;

    /* Make sure that the first argument is acceptable as 'self' */
    if (argc < 1) {
        PyErr_Format(PyExc_TypeError,
                     "descriptor '%V' of '%.100s' "
                     "object needs an argument",
                     descr_name((PyDescrObject *)descr), "?",
                     PyDescr_TYPE(descr)->tp_name);
        return NULL;
    }
    self = stack[0];
    if (!_PyObject_RealIsSubclass((PyObject *)Py_TYPE(self),
                                  (PyObject *)PyDescr_TYPE(descr))) {
        PyErr_Format(PyExc_TypeError,
                     "descriptor '%V' "
                     "requires a '%.100s' object "
                     "but received a '%.100s'",
                     descr_name((PyDescrObject *)descr), "?",
                     PyDescr_TYPE(descr)->tp_name,
                     self->ob_type->tp_name);
        return NULL;
    }

    if (Py_EnterRecursiveCall(" while calling a Python object")) {
        return NULL;
    }

    result = wrapperdescr_raw_call(
        descr, self,
        stack + 1,
        argc - 1,
        kwnames);

    Py_LeaveRecursiveCall();
    return result;
}


static PyObject *
method_get_doc(PyMethodDescrObject *descr, void *closure)
{
    return _PyType_GetDocFromInternalDoc(descr->d_method->ml_name, descr->d_method->ml_doc);
}

static PyObject *
method_get_text_signature(PyMethodDescrObject *descr, void *closure)
{
    return _PyType_GetTextSignatureFromInternalDoc(descr->d_method->ml_name, descr->d_method->ml_doc);
}

static PyObject *
method_get_typed_signature(PyMethodDescrObject *descr, void *closure)
{
    return _PyMethodDef_GetTypedSignature(descr->d_method);
}

static PyObject *
calculate_qualname(PyDescrObject *descr)
{
    PyObject *type_qualname, *res;
    _Py_IDENTIFIER(__qualname__);

    if (descr->d_name == NULL || !PyUnicode_Check(descr->d_name)) {
        PyErr_SetString(PyExc_TypeError,
                        "<descriptor>.__name__ is not a unicode object");
        return NULL;
    }

    type_qualname = _PyObject_GetAttrId((PyObject *)descr->d_type,
                                        &PyId___qualname__);
    if (type_qualname == NULL)
        return NULL;

    if (!PyUnicode_Check(type_qualname)) {
        PyErr_SetString(PyExc_TypeError, "<descriptor>.__objclass__."
                        "__qualname__ is not a unicode object");
        Py_XDECREF(type_qualname);
        return NULL;
    }

    res = PyUnicode_FromFormat("%S.%S", type_qualname, descr->d_name);
    Py_DECREF(type_qualname);
    return res;
}

static PyObject *
descr_get_qualname(PyDescrObject *descr, void *Py_UNUSED(ignored))
{
    if (descr->d_qualname == NULL)
        descr->d_qualname = calculate_qualname(descr);
    Py_XINCREF(descr->d_qualname);
    return descr->d_qualname;
}

static PyObject *
descr_reduce(PyDescrObject *descr, PyObject *Py_UNUSED(ignored))
{
    _Py_IDENTIFIER(getattr);
    return Py_BuildValue("N(OO)", _PyEval_GetBuiltinId(&PyId_getattr),
                         PyDescr_TYPE(descr), PyDescr_NAME(descr));
}

static PyMethodDef descr_methods[] = {
    {"__reduce__", (PyCFunction)descr_reduce, METH_NOARGS, NULL},
    {NULL, NULL}
};

static PyMemberDef descr_members[] = {
    {"__objclass__", T_OBJECT, offsetof(PyDescrObject, d_type), READONLY},
    {"__name__", T_OBJECT, offsetof(PyDescrObject, d_name), READONLY},
    {0}
};

static PyGetSetDef method_getset[] = {
    {"__doc__", (getter)method_get_doc},
    {"__qualname__", (getter)descr_get_qualname},
    {"__text_signature__", (getter)method_get_text_signature},
    {"__typed_signature__", (getter)method_get_typed_signature},
    {0}
};

static PyObject *
member_get_doc(PyMemberDescrObject *descr, void *closure)
{
    if (descr->d_member->doc == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(descr->d_member->doc);
}

static PyGetSetDef member_getset[] = {
    {"__doc__", (getter)member_get_doc},
    {"__qualname__", (getter)descr_get_qualname},
    {0}
};

static PyObject *
getset_get_doc(PyGetSetDescrObject *descr, void *closure)
{
    if (descr->d_getset->doc == NULL) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(descr->d_getset->doc);
}

static PyGetSetDef getset_getset[] = {
    {"__doc__", (getter)getset_get_doc},
    {"__qualname__", (getter)descr_get_qualname},
    {0}
};

static PyObject *
wrapperdescr_get_doc(PyWrapperDescrObject *descr, void *closure)
{
    return _PyType_GetDocFromInternalDoc(descr->d_base->name, descr->d_base->doc);
}

static PyObject *
wrapperdescr_get_text_signature(PyWrapperDescrObject *descr, void *closure)
{
    return _PyType_GetTextSignatureFromInternalDoc(descr->d_base->name, descr->d_base->doc);
}

static PyGetSetDef wrapperdescr_getset[] = {
    {"__doc__", (getter)wrapperdescr_get_doc},
    {"__qualname__", (getter)descr_get_qualname},
    {"__text_signature__", (getter)wrapperdescr_get_text_signature},
    {0}
};

static int
descr_traverse(PyObject *self, visitproc visit, void *arg)
{
    PyDescrObject *descr = (PyDescrObject *)self;
    Py_VISIT(descr->d_type);
    return 0;
}

PyTypeObject PyMethodDescr_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "method_descriptor",
    sizeof(PyMethodDescrObject),
    0,
    (destructor)descr_dealloc,                  /* tp_dealloc */
    offsetof(PyMethodDescrObject, vectorcall),  /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)method_repr,                      /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    PyVectorcall_Call,                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
    _Py_TPFLAGS_HAVE_VECTORCALL |
    Py_TPFLAGS_METHOD_DESCRIPTOR,               /* tp_flags */
    0,                                          /* tp_doc */
    descr_traverse,                             /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    descr_methods,                              /* tp_methods */
    descr_members,                              /* tp_members */
    method_getset,                              /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    (descrgetfunc)method_get,                   /* tp_descr_get */
    0,                                          /* tp_descr_set */
};

/* This is for METH_CLASS in C, not for "f = classmethod(f)" in Python! */
PyTypeObject PyClassMethodDescr_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "classmethod_descriptor",
    sizeof(PyMethodDescrObject),
    0,
    (destructor)descr_dealloc,                  /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)method_repr,                      /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    (ternaryfunc)classmethoddescr_call,         /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    0,                                          /* tp_doc */
    descr_traverse,                             /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    descr_methods,                              /* tp_methods */
    descr_members,                              /* tp_members */
    method_getset,                              /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    (descrgetfunc)classmethod_get,              /* tp_descr_get */
    0,                                          /* tp_descr_set */
};

PyTypeObject PyMemberDescr_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "member_descriptor",
    sizeof(PyMemberDescrObject),
    0,
    (destructor)descr_dealloc,                  /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)member_repr,                      /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    0,                                          /* tp_doc */
    descr_traverse,                             /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    descr_methods,                              /* tp_methods */
    descr_members,                              /* tp_members */
    member_getset,                              /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    (descrgetfunc)member_get,                   /* tp_descr_get */
    (descrsetfunc)member_set,                   /* tp_descr_set */
};

PyTypeObject PyGetSetDescr_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "getset_descriptor",
    sizeof(PyGetSetDescrObject),
    0,
    (destructor)descr_dealloc,                  /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)getset_repr,                      /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    0,                                          /* tp_doc */
    descr_traverse,                             /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    descr_members,                              /* tp_members */
    getset_getset,                              /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    (descrgetfunc)getset_get,                   /* tp_descr_get */
    (descrsetfunc)getset_set,                   /* tp_descr_set */
};

PyTypeObject PyWrapperDescr_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "wrapper_descriptor",
    sizeof(PyWrapperDescrObject),
    0,
    (destructor)descr_dealloc,                  /* tp_dealloc */
    offsetof(PyWrapperDescrObject, d_vectorcall), /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)wrapperdescr_repr,                /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    (ternaryfunc)PyVectorcall_Call,             /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
    Py_TPFLAGS_METHOD_DESCRIPTOR |
    _Py_TPFLAGS_HAVE_VECTORCALL,                /* tp_flags */
    0,                                          /* tp_doc */
    descr_traverse,                             /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    descr_methods,                              /* tp_methods */
    descr_members,                              /* tp_members */
    wrapperdescr_getset,                        /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    (descrgetfunc)wrapperdescr_get,             /* tp_descr_get */
    0,                                          /* tp_descr_set */
};

static PyDescrObject *
descr_new(PyTypeObject *descrtype, PyTypeObject *type, const char *name)
{
    PyDescrObject *descr;

    descr = (PyDescrObject *)PyType_GenericAlloc(descrtype, 0);
    if (descr != NULL) {
        Py_XINCREF(type);
        descr->d_type = type;
        descr->d_name = PyUnicode_InternFromString(name);
        if (descr->d_name == NULL) {
            Py_DECREF(descr);
            descr = NULL;
        }
        else {
            descr->d_qualname = NULL;
        }
    }
    return descr;
}

PyObject *
PyDescr_NewMethod(PyTypeObject *type, PyMethodDef *method)
{
    /* Figure out correct vectorcall function to use */
    vectorcallfunc vectorcall;
    _PyTypedMethodDef *sig;
    switch (method->ml_flags & (METH_VARARGS | METH_FASTCALL | METH_NOARGS | METH_O | METH_KEYWORDS | METH_TYPED))
    {
        case METH_VARARGS:
            vectorcall = method_vectorcall_VARARGS;
            break;
        case METH_VARARGS | METH_KEYWORDS:
            vectorcall = method_vectorcall_VARARGS_KEYWORDS;
            break;
        case METH_FASTCALL:
            vectorcall = method_vectorcall_FASTCALL;
            break;
        case METH_FASTCALL | METH_KEYWORDS:
            vectorcall = method_vectorcall_FASTCALL_KEYWORDS;
            break;
        case METH_NOARGS:
            vectorcall = method_vectorcall_NOARGS;
            break;
        case METH_O:
            vectorcall = method_vectorcall_O;
            break;
        case METH_TYPED:
            sig = (_PyTypedMethodDef *)method->ml_meth;
            Py_ssize_t arg_cnt = 0;
            while (sig->tmd_sig[arg_cnt] != NULL) {
                arg_cnt++;
            }
            switch (arg_cnt) {
                case 0:
                    vectorcall = method_vectorcall_typed_0;
                    break;
                case 1:
                    vectorcall = method_vectorcall_typed_1;
                    break;
                case 2:
                    vectorcall = method_vectorcall_typed_2;
                    break;
                default:
                    PyErr_Format(PyExc_SystemError,
                                "%s() method: unsupported arg count %d for typed method", method->ml_name, arg_cnt);
                    return NULL;
            }
            break;
        default:
            PyErr_Format(PyExc_SystemError,
                         "%s() method: bad call flags", method->ml_name);
            return NULL;
    }

    PyMethodDescrObject *descr;

    descr = (PyMethodDescrObject *)descr_new(&PyMethodDescr_Type,
                                             type, method->ml_name);
    if (descr != NULL) {
        descr->d_method = method;
        descr->vectorcall = vectorcall;
    }
    return (PyObject *)descr;
}

PyObject *
PyDescr_NewClassMethod(PyTypeObject *type, PyMethodDef *method)
{
    PyMethodDescrObject *descr;

    descr = (PyMethodDescrObject *)descr_new(&PyClassMethodDescr_Type,
                                             type, method->ml_name);
    if (descr != NULL)
        descr->d_method = method;
    return (PyObject *)descr;
}

PyObject *
PyDescr_NewMember(PyTypeObject *type, PyMemberDef *member)
{
    PyMemberDescrObject *descr;

    descr = (PyMemberDescrObject *)descr_new(&PyMemberDescr_Type,
                                             type, member->name);
    if (descr != NULL)
        descr->d_member = member;
    return (PyObject *)descr;
}

PyObject *
PyDescr_NewGetSet(PyTypeObject *type, PyGetSetDef *getset)
{
    PyGetSetDescrObject *descr;

    descr = (PyGetSetDescrObject *)descr_new(&PyGetSetDescr_Type,
                                             type, getset->name);
    if (descr != NULL)
        descr->d_getset = getset;
    return (PyObject *)descr;
}

PyObject *
PyDescr_NewWrapper(PyTypeObject *type, struct wrapperbase *base, void *wrapped)
{
    PyWrapperDescrObject *descr;

    descr = (PyWrapperDescrObject *)descr_new(&PyWrapperDescr_Type,
                                             type, base->name);
    if (descr != NULL) {
        descr->d_base = base;
        descr->d_wrapped = wrapped;
        descr->d_vectorcall = (vectorcallfunc)wrapperdescr_vectorcall;
    }
    return (PyObject *)descr;
}


/* --- mappingproxy: read-only proxy for mappings --- */

/* This has no reason to be in this file except that adding new files is a
   bit of a pain */

typedef struct {
    PyObject_HEAD
    PyObject *mapping;
} mappingproxyobject;

static Py_ssize_t
mappingproxy_len(mappingproxyobject *pp)
{
    return PyObject_Size(pp->mapping);
}

static PyObject *
mappingproxy_getitem(mappingproxyobject *pp, PyObject *key)
{
    return PyObject_GetItem(pp->mapping, key);
}

static PyMappingMethods mappingproxy_as_mapping = {
    (lenfunc)mappingproxy_len,                  /* mp_length */
    (binaryfunc)mappingproxy_getitem,           /* mp_subscript */
    0,                                          /* mp_ass_subscript */
};

static int
mappingproxy_contains(mappingproxyobject *pp, PyObject *key)
{
    if (PyDict_CheckExact(pp->mapping))
        return PyDict_Contains(pp->mapping, key);
    else
        return PySequence_Contains(pp->mapping, key);
}

static PySequenceMethods mappingproxy_as_sequence = {
    0,                                          /* sq_length */
    0,                                          /* sq_concat */
    0,                                          /* sq_repeat */
    0,                                          /* sq_item */
    0,                                          /* sq_slice */
    0,                                          /* sq_ass_item */
    0,                                          /* sq_ass_slice */
    (objobjproc)mappingproxy_contains,                 /* sq_contains */
    0,                                          /* sq_inplace_concat */
    0,                                          /* sq_inplace_repeat */
};

static PyObject *
mappingproxy_get(mappingproxyobject *pp, PyObject *args)
{
    PyObject *key, *def = Py_None;
    _Py_IDENTIFIER(get);

    if (!PyArg_UnpackTuple(args, "get", 1, 2, &key, &def))
        return NULL;
    return _PyObject_CallMethodIdObjArgs(pp->mapping, &PyId_get,
                                         key, def, NULL);
}

static PyObject *
mappingproxy_keys(mappingproxyobject *pp, PyObject *Py_UNUSED(ignored))
{
    _Py_IDENTIFIER(keys);
    return _PyObject_CallMethodId(pp->mapping, &PyId_keys, NULL);
}

static PyObject *
mappingproxy_values(mappingproxyobject *pp, PyObject *Py_UNUSED(ignored))
{
    _Py_IDENTIFIER(values);
    return _PyObject_CallMethodId(pp->mapping, &PyId_values, NULL);
}

static PyObject *
mappingproxy_items(mappingproxyobject *pp, PyObject *Py_UNUSED(ignored))
{
    _Py_IDENTIFIER(items);
    return _PyObject_CallMethodId(pp->mapping, &PyId_items, NULL);
}

static PyObject *
mappingproxy_copy(mappingproxyobject *pp, PyObject *Py_UNUSED(ignored))
{
    _Py_IDENTIFIER(copy);
    return _PyObject_CallMethodId(pp->mapping, &PyId_copy, NULL);
}

/* WARNING: mappingproxy methods must not give access
            to the underlying mapping */

static PyMethodDef mappingproxy_methods[] = {
    {"get",       (PyCFunction)mappingproxy_get,        METH_VARARGS,
     PyDoc_STR("D.get(k[,d]) -> D[k] if k in D, else d."
               "  d defaults to None.")},
    {"keys",      (PyCFunction)mappingproxy_keys,       METH_NOARGS,
     PyDoc_STR("D.keys() -> list of D's keys")},
    {"values",    (PyCFunction)mappingproxy_values,     METH_NOARGS,
     PyDoc_STR("D.values() -> list of D's values")},
    {"items",     (PyCFunction)mappingproxy_items,      METH_NOARGS,
     PyDoc_STR("D.items() -> list of D's (key, value) pairs, as 2-tuples")},
    {"copy",      (PyCFunction)mappingproxy_copy,       METH_NOARGS,
     PyDoc_STR("D.copy() -> a shallow copy of D")},
    {0}
};

static void
mappingproxy_dealloc(mappingproxyobject *pp)
{
    _PyObject_GC_UNTRACK(pp);
    Py_DECREF(pp->mapping);
    PyObject_GC_Del(pp);
}

static PyObject *
mappingproxy_getiter(mappingproxyobject *pp)
{
    return PyObject_GetIter(pp->mapping);
}

static PyObject *
mappingproxy_str(mappingproxyobject *pp)
{
    return PyObject_Str(pp->mapping);
}

static PyObject *
mappingproxy_repr(mappingproxyobject *pp)
{
    return PyUnicode_FromFormat("mappingproxy(%R)", pp->mapping);
}

static int
mappingproxy_traverse(PyObject *self, visitproc visit, void *arg)
{
    mappingproxyobject *pp = (mappingproxyobject *)self;
    Py_VISIT(pp->mapping);
    return 0;
}

static PyObject *
mappingproxy_richcompare(mappingproxyobject *v, PyObject *w, int op)
{
    return PyObject_RichCompare(v->mapping, w, op);
}

static int
mappingproxy_check_mapping(PyObject *mapping)
{
    if (!PyMapping_Check(mapping)
        || PyList_Check(mapping)
        || PyTuple_Check(mapping)) {
        PyErr_Format(PyExc_TypeError,
                    "mappingproxy() argument must be a mapping, not %s",
                    Py_TYPE(mapping)->tp_name);
        return -1;
    }
    return 0;
}

/*[clinic input]
@classmethod
mappingproxy.__new__ as mappingproxy_new

    mapping: object

[clinic start generated code]*/

static PyObject *
mappingproxy_new_impl(PyTypeObject *type, PyObject *mapping)
/*[clinic end generated code: output=65f27f02d5b68fa7 input=d2d620d4f598d4f8]*/
{
    mappingproxyobject *mappingproxy;

    if (mappingproxy_check_mapping(mapping) == -1)
        return NULL;

    mappingproxy = PyObject_GC_New(mappingproxyobject, &PyDictProxy_Type);
    if (mappingproxy == NULL)
        return NULL;
    Py_INCREF(mapping);
    mappingproxy->mapping = mapping;
    _PyObject_GC_TRACK(mappingproxy);
    return (PyObject *)mappingproxy;
}

PyObject *
PyDictProxy_New(PyObject *mapping)
{
    mappingproxyobject *pp;

    if (mappingproxy_check_mapping(mapping) == -1)
        return NULL;

    pp = PyObject_GC_New(mappingproxyobject, &PyDictProxy_Type);
    if (pp != NULL) {
        Py_INCREF(mapping);
        pp->mapping = mapping;
        _PyObject_GC_TRACK(pp);
    }
    return (PyObject *)pp;
}


/* --- Wrapper object for "slot" methods --- */

/* This has no reason to be in this file except that adding new files is a
   bit of a pain */

typedef struct {
    PyObject_HEAD
    PyWrapperDescrObject *descr;
    PyObject *self;
    vectorcallfunc vectorcall;
} wrapperobject;

#define Wrapper_Check(v) (Py_TYPE(v) == &_PyMethodWrapper_Type)

static wrapperobject *wrapper_free_list = NULL;
static int wrapper_numfree = 0;
#define wrapper_MAXFREELIST 200

static void
wrapper_dealloc(wrapperobject *wp)
{
    PyObject_GC_UnTrack(wp);
    Py_TRASHCAN_BEGIN(wp, wrapper_dealloc)
    Py_XDECREF(wp->descr);
    Py_XDECREF(wp->self);
    if (wrapper_numfree < wrapper_MAXFREELIST) {
        ++wrapper_numfree;
        wp->descr = (PyWrapperDescrObject *)wrapper_free_list;
        wrapper_free_list = wp;
    } else {
        PyObject_GC_Del(wp);
    }

    Py_TRASHCAN_END
}

static PyObject *
wrapper_richcompare(PyObject *a, PyObject *b, int op)
{
    wrapperobject *wa, *wb;
    int eq;

    assert(a != NULL && b != NULL);

    /* both arguments should be wrapperobjects */
    if ((op != Py_EQ && op != Py_NE)
        || !Wrapper_Check(a) || !Wrapper_Check(b))
    {
        Py_RETURN_NOTIMPLEMENTED;
    }

    wa = (wrapperobject *)a;
    wb = (wrapperobject *)b;
    eq = (wa->descr == wb->descr && wa->self == wb->self);
    if (eq == (op == Py_EQ)) {
        Py_RETURN_TRUE;
    }
    else {
        Py_RETURN_FALSE;
    }
}

static Py_hash_t
wrapper_hash(wrapperobject *wp)
{
    Py_hash_t x, y;
    x = _Py_HashPointer(wp->self);
    y = _Py_HashPointer(wp->descr);
    x = x ^ y;
    if (x == -1)
        x = -2;
    return x;
}

static PyObject *
wrapper_repr(wrapperobject *wp)
{
    return PyUnicode_FromFormat("<method-wrapper '%s' of %s object at %p>",
                               wp->descr->d_base->name,
                               wp->self->ob_type->tp_name,
                               wp->self);
}

static PyObject *
wrapper_reduce(wrapperobject *wp, PyObject *Py_UNUSED(ignored))
{
    _Py_IDENTIFIER(getattr);
    return Py_BuildValue("N(OO)", _PyEval_GetBuiltinId(&PyId_getattr),
                         wp->self, PyDescr_NAME(wp->descr));
}

static PyMethodDef wrapper_methods[] = {
    {"__reduce__", (PyCFunction)wrapper_reduce, METH_NOARGS, NULL},
    {NULL, NULL}
};

static PyMemberDef wrapper_members[] = {
    {"__self__", T_OBJECT, offsetof(wrapperobject, self), READONLY},
    {0}
};

static PyObject *
wrapper_objclass(wrapperobject *wp, void *Py_UNUSED(ignored))
{
    PyObject *c = (PyObject *)PyDescr_TYPE(wp->descr);

    Py_INCREF(c);
    return c;
}

static PyObject *
wrapper_name(wrapperobject *wp, void *Py_UNUSED(ignored))
{
    const char *s = wp->descr->d_base->name;

    return PyUnicode_FromString(s);
}

static PyObject *
wrapper_doc(wrapperobject *wp, void *Py_UNUSED(ignored))
{
    return _PyType_GetDocFromInternalDoc(wp->descr->d_base->name, wp->descr->d_base->doc);
}

static PyObject *
wrapper_text_signature(wrapperobject *wp, void *Py_UNUSED(ignored))
{
    return _PyType_GetTextSignatureFromInternalDoc(wp->descr->d_base->name, wp->descr->d_base->doc);
}

static PyObject *
wrapper_qualname(wrapperobject *wp, void *Py_UNUSED(ignored))
{
    return descr_get_qualname((PyDescrObject *)wp->descr, NULL);
}

static PyGetSetDef wrapper_getsets[] = {
    {"__objclass__", (getter)wrapper_objclass},
    {"__name__", (getter)wrapper_name},
    {"__qualname__", (getter)wrapper_qualname},
    {"__doc__", (getter)wrapper_doc},
    {"__text_signature__", (getter)wrapper_text_signature},
    {0}
};


static PyObject *
wrapper_vectorcall(wrapperobject *wp, PyObject *const *stack, size_t nargsf, PyObject *kwnames)
{
    if (Py_EnterRecursiveCall(" while calling a Python object")) {
        return NULL;
    }

    PyObject *result = wrapperdescr_raw_call(
        wp->descr, wp->self, stack, PyVectorcall_NARGS(nargsf), kwnames);

    Py_LeaveRecursiveCall();
    return result;
}

static int
wrapper_traverse(PyObject *self, visitproc visit, void *arg)
{
    wrapperobject *wp = (wrapperobject *)self;
    Py_VISIT(wp->descr);
    Py_VISIT(wp->self);
    return 0;
}

PyTypeObject _PyMethodWrapper_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "method-wrapper",                           /* tp_name */
    sizeof(wrapperobject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)wrapper_dealloc,                /* tp_dealloc */
    offsetof(wrapperobject, vectorcall),      /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)wrapper_repr,                     /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    (hashfunc)wrapper_hash,                     /* tp_hash */
    (ternaryfunc)PyVectorcall_Call,             /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | _Py_TPFLAGS_HAVE_VECTORCALL, /* tp_flags */
    0,                                          /* tp_doc */
    wrapper_traverse,                           /* tp_traverse */
    0,                                          /* tp_clear */
    wrapper_richcompare,                        /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    wrapper_methods,                            /* tp_methods */
    wrapper_members,                            /* tp_members */
    wrapper_getsets,                            /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
};

PyObject *
PyWrapper_New(PyObject *d, PyObject *self)
{
    wrapperobject *wp;
    PyWrapperDescrObject *descr;

    assert(PyObject_TypeCheck(d, &PyWrapperDescr_Type));
    descr = (PyWrapperDescrObject *)d;
    assert(_PyObject_RealIsSubclass((PyObject *)Py_TYPE(self),
                                    (PyObject *)PyDescr_TYPE(descr)));

    if (wrapper_free_list == NULL) {
        wp = PyObject_GC_New(wrapperobject, &_PyMethodWrapper_Type);
    } else {
        assert(wrapper_numfree > 0);
        --wrapper_numfree;
        wp = wrapper_free_list;
        wrapper_free_list = (wrapperobject *)wrapper_free_list->descr;

        _Py_NewReference((PyObject *)wp);
    }

    if (wp != NULL) {
        Py_INCREF(descr);
        wp->descr = descr;
        Py_INCREF(self);
        wp->self = self;
        wp->vectorcall = (vectorcallfunc)wrapper_vectorcall;
        _PyObject_GC_TRACK(wp);
    }
    return (PyObject *)wp;
}

int
_PyWrapper_ClearFreeList(void)
{
    int freelist_size = wrapper_numfree;

    while (wrapper_free_list != NULL) {
        wrapperobject *wp = wrapper_free_list;
        wrapper_free_list = (wrapperobject *)wrapper_free_list->descr;
        PyObject_GC_Del(wp);
        --wrapper_numfree;
    }
    assert(wrapper_numfree == 0);
    return freelist_size;
}

/* A built-in 'property' type */

/*
class property(object):

    def __init__(self, fget=None, fset=None, fdel=None, doc=None):
        if doc is None and fget is not None and hasattr(fget, "__doc__"):
            doc = fget.__doc__
        self.__get = fget
        self.__set = fset
        self.__del = fdel
        self.__doc__ = doc

    def __get__(self, inst, type=None):
        if inst is None:
            return self
        if self.__get is None:
            raise AttributeError, "unreadable attribute"
        return self.__get(inst)

    def __set__(self, inst, value):
        if self.__set is None:
            raise AttributeError, "can't set attribute"
        return self.__set(inst, value)

    def __delete__(self, inst):
        if self.__del is None:
            raise AttributeError, "can't delete attribute"
        return self.__del(inst)

*/

static PyObject * property_copy(PyObject *, PyObject *, PyObject *,
                                  PyObject *);

static PyMemberDef property_members[] = {
    {"fget", T_OBJECT, offsetof(propertyobject, prop_get), READONLY},
    {"fset", T_OBJECT, offsetof(propertyobject, prop_set), READONLY},
    {"fdel", T_OBJECT, offsetof(propertyobject, prop_del), READONLY},
    {"__doc__",  T_OBJECT, offsetof(propertyobject, prop_doc), 0},
    {0}
};


PyDoc_STRVAR(getter_doc,
             "Descriptor to change the getter on a property.");

static PyObject *
property_getter(PyObject *self, PyObject *getter)
{
    return property_copy(self, getter, NULL, NULL);
}


PyDoc_STRVAR(setter_doc,
             "Descriptor to change the setter on a property.");

static PyObject *
property_setter(PyObject *self, PyObject *setter)
{
    return property_copy(self, NULL, setter, NULL);
}


PyDoc_STRVAR(deleter_doc,
             "Descriptor to change the deleter on a property.");

static PyObject *
property_deleter(PyObject *self, PyObject *deleter)
{
    return property_copy(self, NULL, NULL, deleter);
}


static PyMethodDef property_methods[] = {
    {"getter", property_getter, METH_O, getter_doc},
    {"setter", property_setter, METH_O, setter_doc},
    {"deleter", property_deleter, METH_O, deleter_doc},
    {0}
};


static void
property_dealloc(PyObject *self)
{
    propertyobject *gs = (propertyobject *)self;

    _PyObject_GC_UNTRACK(self);
    Py_XDECREF(gs->prop_get);
    Py_XDECREF(gs->prop_set);
    Py_XDECREF(gs->prop_del);
    Py_XDECREF(gs->prop_doc);
    self->ob_type->tp_free(self);
}

static PyObject *
property_descr_get(PyObject *self, PyObject *obj, PyObject *type)
{
    if (obj == NULL || obj == Py_None) {
        Py_INCREF(self);
        return self;
    }

    propertyobject *gs = (propertyobject *)self;
    if (gs->prop_get == NULL) {
        PyErr_SetString(PyExc_AttributeError, "unreadable attribute");
        return NULL;
    }

    PyObject *args[1] = {obj};
    return _PyObject_FastCall(gs->prop_get, args, 1);
}

static int
property_descr_set(PyObject *self, PyObject *obj, PyObject *value)
{
    propertyobject *gs = (propertyobject *)self;
    PyObject *func, *res;

    if (value == NULL)
        func = gs->prop_del;
    else
        func = gs->prop_set;
    if (func == NULL) {
        PyErr_SetString(PyExc_AttributeError,
                        value == NULL ?
                        "can't delete attribute" :
                "can't set attribute");
        return -1;
    }
    if (value == NULL)
        res = _PyObject_Call1Arg(func, obj);
    else {
        PyObject *args[2] = {obj, value};
        res = _PyObject_Vectorcall(func, args, 2, NULL);
    }
    if (res == NULL)
        return -1;
    Py_DECREF(res);
    return 0;
}

static PyObject *
property_copy(PyObject *old, PyObject *get, PyObject *set, PyObject *del)
{
    propertyobject *pold = (propertyobject *)old;
    PyObject *new, *type, *doc;

    type = PyObject_Type(old);
    if (type == NULL)
        return NULL;

    if (get == NULL || get == Py_None) {
        Py_XDECREF(get);
        get = pold->prop_get ? pold->prop_get : Py_None;
    }
    if (set == NULL || set == Py_None) {
        Py_XDECREF(set);
        set = pold->prop_set ? pold->prop_set : Py_None;
    }
    if (del == NULL || del == Py_None) {
        Py_XDECREF(del);
        del = pold->prop_del ? pold->prop_del : Py_None;
    }
    if (pold->getter_doc && get != Py_None) {
        /* make _init use __doc__ from getter */
        doc = Py_None;
    }
    else {
        doc = pold->prop_doc ? pold->prop_doc : Py_None;
    }

    new =  PyObject_CallFunctionObjArgs(type, get, set, del, doc, NULL);
    Py_DECREF(type);
    if (new == NULL)
        return NULL;
    return new;
}

/*[clinic input]
property.__init__ as property_init

    fget: object(c_default="NULL") = None
        function to be used for getting an attribute value
    fset: object(c_default="NULL") = None
        function to be used for setting an attribute value
    fdel: object(c_default="NULL") = None
        function to be used for del'ing an attribute
    doc: object(c_default="NULL") = None
        docstring

Property attribute.

Typical use is to define a managed attribute x:

class C(object):
    def getx(self): return self._x
    def setx(self, value): self._x = value
    def delx(self): del self._x
    x = property(getx, setx, delx, "I'm the 'x' property.")

Decorators make defining new properties or modifying existing ones easy:

class C(object):
    @property
    def x(self):
        "I am the 'x' property."
        return self._x
    @x.setter
    def x(self, value):
        self._x = value
    @x.deleter
    def x(self):
        del self._x
[clinic start generated code]*/

static int
property_init_impl(propertyobject *self, PyObject *fget, PyObject *fset,
                   PyObject *fdel, PyObject *doc)
/*[clinic end generated code: output=01a960742b692b57 input=dfb5dbbffc6932d5]*/
{
    if (fget == Py_None)
        fget = NULL;
    if (fset == Py_None)
        fset = NULL;
    if (fdel == Py_None)
        fdel = NULL;

    Py_XINCREF(fget);
    Py_XINCREF(fset);
    Py_XINCREF(fdel);
    Py_XINCREF(doc);

    Py_XSETREF(self->prop_get, fget);
    Py_XSETREF(self->prop_set, fset);
    Py_XSETREF(self->prop_del, fdel);
    Py_XSETREF(self->prop_doc, doc);
    self->getter_doc = 0;

    /* if no docstring given and the getter has one, use that one */
    if ((doc == NULL || doc == Py_None) && fget != NULL) {
        _Py_IDENTIFIER(__doc__);
        PyObject *get_doc;
        int rc = _PyObject_LookupAttrId(fget, &PyId___doc__, &get_doc);
        if (rc <= 0) {
            return rc;
        }
        if (Py_TYPE(self) == &PyProperty_Type) {
            Py_XSETREF(self->prop_doc, get_doc);
        }
        else {
            /* If this is a property subclass, put __doc__
               in dict of the subclass instance instead,
               otherwise it gets shadowed by __doc__ in the
               class's dict. */
            int err = _PyObject_SetAttrId((PyObject *)self, &PyId___doc__, get_doc);
            Py_DECREF(get_doc);
            if (err < 0)
                return -1;
        }
        self->getter_doc = 1;
    }

    return 0;
}

static PyObject *
property_get___isabstractmethod__(propertyobject *prop, void *closure)
{
    int res = _PyObject_IsAbstract(prop->prop_get);
    if (res == -1) {
        return NULL;
    }
    else if (res) {
        Py_RETURN_TRUE;
    }

    res = _PyObject_IsAbstract(prop->prop_set);
    if (res == -1) {
        return NULL;
    }
    else if (res) {
        Py_RETURN_TRUE;
    }

    res = _PyObject_IsAbstract(prop->prop_del);
    if (res == -1) {
        return NULL;
    }
    else if (res) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyGetSetDef property_getsetlist[] = {
    {"__isabstractmethod__",
     (getter)property_get___isabstractmethod__, NULL,
     NULL,
     NULL},
    {NULL} /* Sentinel */
};

static int
property_traverse(PyObject *self, visitproc visit, void *arg)
{
    propertyobject *pp = (propertyobject *)self;
    Py_VISIT(pp->prop_get);
    Py_VISIT(pp->prop_set);
    Py_VISIT(pp->prop_del);
    Py_VISIT(pp->prop_doc);
    return 0;
}

static int
property_clear(PyObject *self)
{
    propertyobject *pp = (propertyobject *)self;
    Py_CLEAR(pp->prop_doc);
    return 0;
}

#include "clinic/descrobject.c.h"

PyTypeObject PyDictProxy_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "mappingproxy",                             /* tp_name */
    sizeof(mappingproxyobject),                 /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)mappingproxy_dealloc,           /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)mappingproxy_repr,                /* tp_repr */
    0,                                          /* tp_as_number */
    &mappingproxy_as_sequence,                  /* tp_as_sequence */
    &mappingproxy_as_mapping,                   /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    (reprfunc)mappingproxy_str,                 /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    0,                                          /* tp_doc */
    mappingproxy_traverse,                      /* tp_traverse */
    0,                                          /* tp_clear */
    (richcmpfunc)mappingproxy_richcompare,      /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    (getiterfunc)mappingproxy_getiter,          /* tp_iter */
    0,                                          /* tp_iternext */
    mappingproxy_methods,                       /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    mappingproxy_new,                           /* tp_new */
};

PyTypeObject PyProperty_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "property",                                 /* tp_name */
    sizeof(propertyobject),                     /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    property_dealloc,                           /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,                    /* tp_flags */
    property_init__doc__,                       /* tp_doc */
    property_traverse,                          /* tp_traverse */
    (inquiry)property_clear,                    /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    property_methods,                           /* tp_methods */
    property_members,                           /* tp_members */
    property_getsetlist,                        /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    property_descr_get,                         /* tp_descr_get */
    property_descr_set,                         /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    property_init,                              /* tp_init */
    PyType_GenericAlloc,                        /* tp_alloc */
    PyType_GenericNew,                          /* tp_new */
    PyObject_GC_Del,                            /* tp_free */
};

/* fb t46346203 */

typedef struct {
    PyObject_HEAD
    PyObject *func;             /* function object */
    PyObject *name;             /* str or member descriptor object */
    PyObject *value;            /* value or NULL when uninitialized */
} PyCachedClassPropertyDescrObject;

static int
cached_classproperty_traverse(PyCachedClassPropertyDescrObject *prop, visitproc visit, void *arg) {
    Py_VISIT(prop->func);
    Py_VISIT(prop->value);
    return 0;
}

static PyObject *
cached_classproperty_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *func;
    PyCachedClassPropertyDescrObject *descr;

    if (PyTuple_GET_SIZE(args) != 1) {
        PyErr_SetString(PyExc_TypeError, "cached_classproperty: 1 argument expected");
        return NULL;
    }

    func = PyTuple_GET_ITEM(args, 0);

    descr = (PyCachedClassPropertyDescrObject *)PyType_GenericAlloc(type, 0);
    if (descr != NULL) {
        PyObject *name;
        if (PyFunction_Check(func)) {
            name = ((PyFunctionObject*)func)->func_name;
        } else {
            name = PyObject_GetAttrString(func, "__name__");
            if (name == NULL) {
                Py_DECREF(descr);
                return NULL;
            }
        }
        descr->func = func;
        descr->name = name;
        Py_INCREF(func);
        Py_INCREF(name);
    }
    return (PyObject *)descr;
}

static PyObject *
cached_classproperty_get(PyObject *self, PyObject *obj, PyObject *cls)
{
    PyCachedClassPropertyDescrObject *cp = (PyCachedClassPropertyDescrObject *)self;
    PyObject *res;

    res = cp->value;
    if (res == NULL) {
        res = _PyObject_Vectorcall(cp->func, &cls, 1, NULL);
        if (res == NULL) {
            return NULL;
        }
        if (cp->value == NULL) {
            /* we steal the ref count */
            cp->value = res;
        } else {
            /* first value to return wins */
            Py_DECREF(res);
            res = cp->value;
        }
    }

    Py_INCREF(res);
    return res;
}

static void
cached_classproperty_dealloc(PyCachedClassPropertyDescrObject *cp)
{
    PyObject_GC_UnTrack(cp);
    Py_XDECREF(cp->func);
    Py_XDECREF(cp->name);
    Py_XDECREF(cp->value);
    PyTypeObject *type = Py_TYPE(cp);
    Py_TYPE(cp)->tp_free(cp);
    Py_DECREF(type);
}

static PyObject *
cached_classproperty_get___doc__(PyCachedClassPropertyDescrObject *cp, void *closure)
{
    PyObject *res = ((PyFunctionObject*)cp->func)->func_doc;
    Py_INCREF(res);
    return res;
}

static PyObject *
cached_classproperty_get_name(PyCachedClassPropertyDescrObject *cp, void *closure)
{
    PyObject *res = cp->name;
    Py_INCREF(res);
    return res;
}

static PyGetSetDef cached_classproperty_getsetlist[] = {
    {"__doc__", (getter)cached_classproperty_get___doc__, NULL, NULL, NULL},
    {"name", (getter)cached_classproperty_get_name, NULL, NULL, NULL},
    {"__name__", (getter)cached_classproperty_get_name, NULL, NULL, NULL},
    {NULL} /* Sentinel */
};

static PyMemberDef cached_classproperty_members[] = {
    {"func", T_OBJECT, offsetof(PyCachedClassPropertyDescrObject, func), READONLY},
    {0}
};

static PyType_Slot PyCachedClassProperty_slots[] = {
    {Py_tp_dealloc, cached_classproperty_dealloc},
    {Py_tp_doc,
    "cached_classproperty(function) --> cached_property object\n\
\n\
Provides a cached class property.  Works with normal types and frozen types\n\
to create values on demand and cache them in the class."},
    {Py_tp_traverse, cached_classproperty_traverse},
    {Py_tp_descr_get, cached_classproperty_get},
    {Py_tp_members, cached_classproperty_members},
    {Py_tp_getset, cached_classproperty_getsetlist},
    {Py_tp_new, cached_classproperty_new},
    {Py_tp_alloc, PyType_GenericAlloc},
    {Py_tp_free, PyObject_GC_Del},
    {0, 0},
};

PyType_Spec _PyCachedClassProperty_TypeSpec = {
    "builtins.cached_classproperty",
    sizeof(PyCachedClassPropertyDescrObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    PyCachedClassProperty_slots
};

/* end fb t46346203 */

/* fb t46346203 */

static int
cached_property_traverse(PyCachedPropertyDescrObject *prop, visitproc visit, void *arg)
{
    Py_VISIT(prop->func);
    Py_VISIT(prop->name_or_descr);
    return 0;
}


PyTypeObject PyCachedProperty_Type;
PyTypeObject PyCachedPropertyWithDescr_Type;

static int
cached_property_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyCachedPropertyDescrObject *cp = (PyCachedPropertyDescrObject *)self;
    PyObject *name_or_descr, *func;

    if (PyTuple_GET_SIZE(args) != 1 && PyTuple_GET_SIZE(args) != 2) {
        PyErr_SetString(PyExc_TypeError, "cached_property: 1 or 2 arguments expected");
        return -1;
    }

    func = PyTuple_GET_ITEM(args, 0);

    if (PyTuple_GET_SIZE(args) == 2) {
        PyMemberDescrObject *descr;
        name_or_descr = PyTuple_GET_ITEM(args, 1);

        if (Py_TYPE(name_or_descr) != &PyMemberDescr_Type) {
            PyErr_SetString(PyExc_TypeError, "cached_property: member descriptor expected for 2nd argument");
            return -1;
        }

        descr = (PyMemberDescrObject *)name_or_descr;
        if(descr->d_member->type != T_OBJECT_EX ||
           descr->d_member->flags) {
           PyErr_SetString(PyExc_TypeError, "cached_property: incompatible descriptor");
           return -1;
        }

        /* change our type to enable setting the cached property, we don't allow
         * subtypes because we can't change their type, and the descriptor would
         * need to account for doing the lookup, and we'd need to dynamically
         * create a subtype of them too, not to mention dealing with extra ref
         * counting on the types */
        if (Py_TYPE(self) != &PyCachedProperty_Type &&
            Py_TYPE(self) != &PyCachedPropertyWithDescr_Type) {
            PyErr_SetString(
                PyExc_TypeError,
                "cached_property: descr cannot be used with subtypes of cached_property");
            return -1;
        }

        Py_TYPE(self) = &PyCachedPropertyWithDescr_Type;
    } else if (PyFunction_Check(func)) {
        name_or_descr = ((PyFunctionObject *)func)->func_name;
    } else {
        name_or_descr = PyObject_GetAttrString(func, "__name__");
        if (name_or_descr == NULL) {
            return -1;
        }
    }

    cp->func = func;
    cp->name_or_descr = name_or_descr;
    Py_INCREF(func);
    Py_INCREF(name_or_descr);

    return 0;
}

PyDoc_STRVAR(cached_property_doc,
"cached_property(function, [slot]) --> cached_property object\n\
\n\
Creates a new cached property where function will be called to produce\n\
the value on the first access.\n\
\n\
If slot descriptor is provided it will be used for storing the value.");


static PyObject *
cached_property_get(PyObject *self, PyObject *obj, PyObject *cls)
{
    PyObject *res, *dict;
    PyCachedPropertyDescrObject *cp = (PyCachedPropertyDescrObject *)self;
    PyObject *stack[1] = {obj};

    if (obj == NULL) {
        Py_INCREF(self);
        return self;
    }

    if (Py_TYPE(cp->name_or_descr) == &PyMemberDescr_Type) {
        PyObject **addr;
        PyMemberDescrObject *descr = (PyMemberDescrObject *)cp->name_or_descr;

        if (Py_TYPE(obj) != PyDescr_TYPE(descr) &&
           !PyObject_TypeCheck(obj, PyDescr_TYPE(descr))) {
            PyErr_Format(PyExc_TypeError,
                         "descriptor '%V' for '%s' objects "
                         "doesn't apply to '%s' object",
                         ((PyDescrObject*)descr)->d_name, "?",
                         PyDescr_TYPE(descr)->tp_name,
                         Py_TYPE(obj)->tp_name);
             return NULL;
        }

        addr = (PyObject **)(((const char *)obj) + descr->d_member->offset);
        res = *addr;
        if (res != NULL) {
            Py_INCREF(res);
            return res;
        }

        res = _PyObject_Vectorcall(cp->func, stack, 1, NULL);
        if (res == NULL) {
            return NULL;
        }

        *addr = res;
        Py_INCREF(res);
    } else {
        dict = PyObject_GenericGetDict(obj, NULL);
        if (dict == NULL) {
            return NULL;
        }

        res = PyDict_GetItem(dict, cp->name_or_descr);
        Py_DECREF(dict);
        if (res != NULL) {
            Py_INCREF(res); /* we got a borrowed ref */
            return res;
        }

        res = _PyObject_Vectorcall(cp->func, stack, 1, NULL);
        if (res == NULL) {
            return NULL;
        }

        if (_PyObjectDict_SetItem(Py_TYPE(obj), _PyObject_GetDictPtr(obj), cp->name_or_descr, res)) {
            Py_DECREF(res);
            return NULL;
        }
    }

    return res;
}

static int
cached_property_set(PyObject *self, PyObject *obj, PyObject *value)
{
    PyCachedPropertyDescrObject *cp = (PyCachedPropertyDescrObject *)self;
    PyObject **dictptr;

    if (Py_TYPE(cp->name_or_descr) == &PyMemberDescr_Type) {
        return Py_TYPE(cp->name_or_descr)->tp_descr_set(cp->name_or_descr, obj, value);
    }

    dictptr = _PyObject_GetDictPtr(obj);

    if (dictptr == NULL) {
        PyErr_SetString(PyExc_AttributeError,
                        "This object has no __dict__");
        return -1;
    }

    return _PyObjectDict_SetItem(Py_TYPE(obj), dictptr, cp->name_or_descr, value);
}

static void
cached_property_dealloc(PyCachedPropertyDescrObject *cp)
{
    PyObject_GC_UnTrack(cp);
    Py_XDECREF(cp->func);
    Py_XDECREF(cp->name_or_descr);
    Py_TYPE(cp)->tp_free(cp);
}

static PyObject *
cached_property_get___doc__(PyCachedPropertyDescrObject *cp, void *closure)
{
    PyObject *res = ((PyFunctionObject*)cp->func)->func_doc;
    Py_INCREF(res);
    return res;
}

static PyObject *
cached_property_get_name(PyCachedPropertyDescrObject *cp, void *closure)
{
    PyObject *res;

    if (Py_TYPE(cp->name_or_descr) != &PyMemberDescr_Type) {
        res = cp->name_or_descr;
    } else {
        res = PyDescr_NAME(cp->name_or_descr);
    }
    Py_INCREF(res);
    return res;
}

static PyObject *
cached_property_get_slot(PyCachedPropertyDescrObject *cp, void *closure)
{
    if (Py_TYPE(cp->name_or_descr) == &PyMemberDescr_Type) {
        PyObject *res = cp->name_or_descr;

        Py_INCREF(res);
        return res;
    }
    Py_RETURN_NONE;
}

static PyGetSetDef cached_property_getsetlist[] = {
    {"__doc__", (getter)cached_property_get___doc__, NULL, NULL, NULL},
    {"__name__", (getter)cached_property_get_name, NULL, NULL, NULL},
    {"name", (getter)cached_property_get_name, NULL, NULL, NULL},
    {"slot", (getter)cached_property_get_slot, NULL, NULL, NULL},
    {NULL} /* Sentinel */
};

static PyMemberDef cached_property_members[] = {
    {"func", T_OBJECT, offsetof(PyCachedPropertyDescrObject, func), READONLY},
    /* currently duplicated until all consumers are updated in favor of fget */
    {"fget", T_OBJECT, offsetof(PyCachedPropertyDescrObject, func), READONLY},
    {0}
};


PyTypeObject PyCachedProperty_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "cached_property",
    .tp_basicsize = sizeof(PyCachedPropertyDescrObject),
    .tp_dealloc =  (destructor)cached_property_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,
    .tp_doc = cached_property_doc,
    .tp_traverse = (traverseproc)cached_property_traverse,
    .tp_descr_get = cached_property_get,
    .tp_members = cached_property_members,
    .tp_getset = cached_property_getsetlist,
    .tp_new = PyType_GenericNew,
    .tp_init = cached_property_init,
    .tp_alloc = PyType_GenericAlloc,
    .tp_free = PyObject_GC_Del,
};

PyTypeObject PyCachedPropertyWithDescr_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "cached_property_with_descr",
    .tp_base = &PyCachedProperty_Type,
    .tp_basicsize = sizeof(PyCachedPropertyDescrObject),
    .tp_dealloc =  (destructor)cached_property_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,
    .tp_doc = cached_property_doc,
    .tp_traverse = (traverseproc)cached_property_traverse,
    .tp_descr_get = cached_property_get,
    .tp_descr_set = cached_property_set,
    .tp_members = cached_property_members,
    .tp_getset = cached_property_getsetlist,
    .tp_new = PyType_GenericNew,
    .tp_init = cached_property_init,
    .tp_alloc = PyType_GenericAlloc,
    .tp_free = PyObject_GC_Del,
};

/* end fb t46346203 */

/* fb T82701047 */
/*********************** AsyncCachedProperty*********************/
static PyObject *_AsyncLazyValue_Type;

static int
async_cached_property_traverse(PyAsyncCachedPropertyDescrObject *prop, visitproc visit, void *arg)
{
    Py_VISIT(prop->func);
    Py_VISIT(prop->name_or_descr);
    return 0;
}

/*[clinic input]

async_cached_property.__init__ as async_cached_property_init

  self: self(type="PyAsyncCachedPropertyDescrObject *")
  func: object(subclass_of="&PyFunction_Type")
  name_or_descr: object(c_default="NULL", subclass_of="&PyMemberDescr_Type") = None

init a async_cached_property.

Creates a new async cached property where function will be called to produce
the async lazy value on the first access.

If slot descriptor is provided it will be used for storing the value."
[clinic start generated code]*/

static int
async_cached_property_init_impl(PyAsyncCachedPropertyDescrObject *self,
                                PyObject *func, PyObject *name_or_descr)
/*[clinic end generated code: output=d8f17f423e7ad7f2 input=1afd71a95b7e8615]*/
{
    if (name_or_descr != NULL) {
        PyMemberDescrObject *descr = (PyMemberDescrObject *)name_or_descr;
        if(descr->d_member->type != T_OBJECT_EX ||
           descr->d_member->flags) {
           PyErr_SetString(PyExc_TypeError, "async_cached_property: incompatible descriptor");
           return -1;
        }
        self->name_or_descr = name_or_descr;
    } else if (PyFunction_Check(func)) {
        self->name_or_descr = ((PyFunctionObject *)func)->func_name;
    } else {
        self->name_or_descr = PyObject_GetAttrString(func, "__name__");
        if (self->name_or_descr == NULL) {
            return -1;
        }
    }

    self->func = func;
    Py_INCREF(self->func);
    Py_INCREF(self->name_or_descr);

    return 0;
}

static inline int import_async_lazy_value() {
    if (_AsyncLazyValue_Type == NULL) {
        _Py_IDENTIFIER(AsyncLazyValue);
        PyObject *asyncio = PyImport_ImportModule("_asyncio");
        if (asyncio == NULL) {
            return -1;
        }
        _AsyncLazyValue_Type = _PyObject_GetAttrId(asyncio, &PyId_AsyncLazyValue);
        Py_DECREF(asyncio);
        if (_AsyncLazyValue_Type == NULL) {
            return -1;
        }
    }
    return 0;
}

static PyObject *
async_cached_property_get(PyObject *self, PyObject *obj, PyObject *cls)
{
    PyObject *res;
    PyAsyncCachedPropertyDescrObject *cp = (PyAsyncCachedPropertyDescrObject *)self;

    if (obj == NULL) {
        Py_INCREF(self);
        return self;
    }

    if (Py_TYPE(cp->name_or_descr) == &PyMemberDescr_Type) {
        PyObject **addr;
        PyMemberDescrObject *descr = (PyMemberDescrObject *)cp->name_or_descr;

        if (Py_TYPE(obj) != PyDescr_TYPE(descr) &&
           !PyObject_TypeCheck(obj, PyDescr_TYPE(descr))) {
            PyErr_Format(PyExc_TypeError,
                         "descriptor '%V' for '%s' objects "
                         "doesn't apply to '%s' object",
                         ((PyDescrObject*)descr)->d_name, "?",
                         PyDescr_TYPE(descr)->tp_name,
                         Py_TYPE(obj)->tp_name);
             return NULL;
        }

        addr = (PyObject **)(((const char *)obj) + descr->d_member->offset);
        res = *addr;
        if (res != NULL) {
            Py_INCREF(res);
            return res;
        }
        if (import_async_lazy_value() < 0) {
            return NULL;
        }
        res = PyObject_CallFunctionObjArgs(_AsyncLazyValue_Type, cp->func, obj, NULL);
        if (res == NULL) {
            return NULL;
        }

        *addr = res;
        Py_INCREF(res);
    } else {
        if (import_async_lazy_value() < 0) {
            return NULL;
        }

        res = PyObject_CallFunctionObjArgs(_AsyncLazyValue_Type, cp->func, obj, NULL);
        if (res == NULL) {
            return NULL;
        }

        if (PyObject_SetAttr(obj, cp->name_or_descr, res) < 0) {
            Py_DECREF(res);
            return NULL;
        }
    }

    return res;
}

static void
async_cached_property_dealloc(PyAsyncCachedPropertyDescrObject *cp)
{
    PyObject_GC_UnTrack(cp);
    Py_XDECREF(cp->func);
    Py_XDECREF(cp->name_or_descr);
    Py_TYPE(cp)->tp_free(cp);
}

static PyObject *
async_cached_property_get___doc__(PyAsyncCachedPropertyDescrObject *cp, void *closure)
{
    PyObject *res = ((PyFunctionObject*)cp->func)->func_doc;
    Py_INCREF(res);
    return res;
}

static PyObject *
async_cached_property_get_name(PyAsyncCachedPropertyDescrObject *cp, void *closure)
{
    PyObject *res;

    if (Py_TYPE(cp->name_or_descr) != &PyMemberDescr_Type) {
        res = cp->name_or_descr;
    } else {
        res = PyDescr_NAME(cp->name_or_descr);
    }
    Py_INCREF(res);
    return res;
}

static PyObject *
async_cached_property_get_slot(PyAsyncCachedPropertyDescrObject *cp, void *closure)
{
    if (Py_TYPE(cp->name_or_descr) == &PyMemberDescr_Type) {
        PyObject *res = cp->name_or_descr;

        Py_INCREF(res);
        return res;
    }
    Py_RETURN_NONE;
}

static PyGetSetDef async_cached_property_getsetlist[] = {
    {"__doc__", (getter)async_cached_property_get___doc__, NULL, NULL, NULL},
    {"name", (getter)async_cached_property_get_name, NULL, NULL, NULL},
    {"slot", (getter)async_cached_property_get_slot, NULL, NULL, NULL},
    {NULL} /* Sentinel */
};

static PyMemberDef async_cached_property_members[] = {
    {"func", T_OBJECT, offsetof(PyAsyncCachedPropertyDescrObject, func), READONLY},
    {"fget", T_OBJECT, offsetof(PyAsyncCachedPropertyDescrObject, func), READONLY},
    {0}
};


PyTypeObject PyAsyncCachedProperty_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "async_cached_property",
    .tp_basicsize = sizeof(PyAsyncCachedPropertyDescrObject),
    .tp_dealloc =  (destructor)async_cached_property_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,
    .tp_doc = async_cached_property_init__doc__,
    .tp_traverse = (traverseproc)async_cached_property_traverse,
    .tp_descr_get = async_cached_property_get,
    .tp_members = async_cached_property_members,
    .tp_getset = async_cached_property_getsetlist,
    .tp_new = PyType_GenericNew,
    .tp_init = async_cached_property_init,
    .tp_alloc = PyType_GenericAlloc,
    .tp_free = PyObject_GC_Del,
};

/* end fb T82701047 */
/* fb T82701047 */
/*********************** AsyncCachedClassProperty*********************/
static int
async_cached_classproperty_traverse(PyAsyncCachedClassPropertyDescrObject *prop, visitproc visit, void *arg) {
    Py_VISIT(prop->func);
    Py_VISIT(prop->value);
    return 0;
}
/*[clinic input]
@classmethod
async_cached_classproperty.__new__ as async_cached_classproperty_new

  func: object(subclass_of="&PyFunction_Type")

Provides an async cached class property.

Works with normal types and frozen types to create values on demand
and cache them in the class.

[clinic start generated code]*/

static PyObject *
async_cached_classproperty_new_impl(PyTypeObject *type, PyObject *func)
/*[clinic end generated code: output=b7972e5345764116 input=056050fde0415935]*/
{
    PyAsyncCachedClassPropertyDescrObject *descr;
    descr = (PyAsyncCachedClassPropertyDescrObject *)PyType_GenericAlloc(type, 0);
    if (descr != NULL) {
        PyObject *name = ((PyFunctionObject*)func)->func_name;
        descr->func = func;
        descr->name = name;
        Py_INCREF(func);
        Py_INCREF(name);
    }
    return (PyObject *)descr;
}

static PyObject *
async_cached_classproperty_get(PyObject *self, PyObject *obj, PyObject *cls)
{
    PyAsyncCachedClassPropertyDescrObject *cp = (PyAsyncCachedClassPropertyDescrObject *)self;
    PyObject *res;

    res = cp->value;
    if (res == NULL) {
        if (import_async_lazy_value() < 0) {
            return NULL;
        }
        res = PyObject_CallFunctionObjArgs(_AsyncLazyValue_Type, cp->func, cls, NULL);
        if (res == NULL) {
            return NULL;
        }
        if (cp->value == NULL) {
            /* we steal the ref count */
            cp->value = res;
        } else {
            /* first value to return wins */
            Py_DECREF(res);
            res = cp->value;
        }
    }

    Py_INCREF(res);
    return res;
}

static void
async_cached_classproperty_dealloc(PyAsyncCachedClassPropertyDescrObject *cp)
{
    PyObject_GC_UnTrack(cp);
    Py_XDECREF(cp->func);
    Py_XDECREF(cp->name);
    Py_XDECREF(cp->value);
    Py_TYPE(cp)->tp_free(cp);
}

static PyObject *
async_cached_classproperty_get___doc__(PyAsyncCachedClassPropertyDescrObject *cp, void *closure)
{
    PyObject *res = ((PyFunctionObject*)cp->func)->func_doc;
    Py_INCREF(res);
    return res;
}

static PyObject *
async_cached_classproperty_get_name(PyAsyncCachedClassPropertyDescrObject *cp, void *closure)
{
    PyObject *res = cp->name;
    Py_INCREF(res);
    return res;
}

static PyGetSetDef async_cached_classproperty_getsetlist[] = {
    {"__doc__", (getter)async_cached_classproperty_get___doc__, NULL, NULL, NULL},
    {"name", (getter)async_cached_classproperty_get_name, NULL, NULL, NULL},
    {NULL} /* Sentinel */
};

static PyMemberDef async_cached_classproperty_members[] = {
    {"func", T_OBJECT, offsetof(PyAsyncCachedClassPropertyDescrObject, func), READONLY},
    {0}
};

PyTypeObject PyAsyncCachedClassProperty_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "async_cached_classproperty",
    .tp_basicsize = sizeof(PyAsyncCachedClassPropertyDescrObject),
    .tp_dealloc =  (destructor)async_cached_classproperty_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .tp_doc = async_cached_classproperty_new__doc__,
    .tp_traverse = (traverseproc)async_cached_classproperty_traverse,
    .tp_descr_get = async_cached_classproperty_get,
    .tp_members = async_cached_classproperty_members,
    .tp_getset = async_cached_classproperty_getsetlist,
    .tp_new = async_cached_classproperty_new,
    .tp_alloc = PyType_GenericAlloc,
    .tp_free = PyObject_GC_Del,
};

/* end fb T82701047 */
