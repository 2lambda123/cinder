/* Function object interface */
#ifndef Py_LIMITED_API
#ifndef Py_FUNCOBJECT_H
#define Py_FUNCOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

/* Function objects and code objects should not be confused with each other:
 *
 * Function objects are created by the execution of the 'def' statement.
 * They reference a code object in their __code__ attribute, which is a
 * purely syntactic object, i.e. nothing more than a compiled version of some
 * source code lines.  There is one code object per source code "fragment",
 * but each code object can be referenced by zero or many function objects
 * depending only on how many times the 'def' statement in the source was
 * executed so far.
 */

 /* FB_entry_BEGIN */
struct PyFunctionObject;
typedef struct PyFunctionObject PyFunctionObject;
struct PyCodeObject;
typedef struct PyCodeObject PyCodeObject;

PyObject *PyEntry_LazyInit(PyFunctionObject *func,
                           PyObject **stack,
                           Py_ssize_t nargsf,
                           PyObject *kwnames);

void PyEntry_init(PyFunctionObject *func);
/* FB_entry_END */

struct PyFunctionObject {
    PyObject_HEAD
    PyObject *func_code;        /* A code object, the __code__ attribute */
    PyObject *func_globals;     /* A dictionary (other mappings won't do) */
    PyObject *func_defaults;    /* NULL or a tuple */
    PyObject *func_kwdefaults;  /* NULL or a dict */
    PyObject *func_closure;     /* NULL or a tuple of cell objects */
    PyObject *func_doc;         /* The __doc__ attribute, can be anything */
    PyObject *func_name;        /* The __name__ attribute, a string object */
    PyObject *func_dict;        /* The __dict__ attribute, a dict or NULL */
    PyObject *func_weakreflist; /* List of weak references */
    PyObject *func_module;      /* The __module__ attribute, can be anything */
    PyObject *func_annotations; /* Annotations, a dict or NULL */
    PyObject *func_qualname;    /* The qualified name */
    vectorcallfunc vectorcall;
    uint64_t readonly_mask;


    /* Invariant:
     *     func_closure contains the bindings for func_code->co_freevars, so
     *     PyTuple_Size(func_closure) == PyCode_GetNumFree(func_code)
     *     (func_closure may be NULL if PyCode_GetNumFree(func_code) == 0).
     */

};

PyAPI_DATA(PyTypeObject) PyFunction_Type;

#define PyFunction_Check(op) (Py_TYPE(op) == &PyFunction_Type)

PyAPI_FUNC(PyObject *) PyFunction_New(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_NewWithQualName(PyObject *, PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_GetCode(PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_GetGlobals(PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_GetModule(PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_GetDefaults(PyObject *);
PyAPI_FUNC(int) PyFunction_SetDefaults(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_GetKwDefaults(PyObject *);
PyAPI_FUNC(int) PyFunction_SetKwDefaults(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_GetClosure(PyObject *);
PyAPI_FUNC(int) PyFunction_SetClosure(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyFunction_GetAnnotations(PyObject *);
PyAPI_FUNC(int) PyFunction_SetAnnotations(PyObject *, PyObject *);

#ifndef Py_LIMITED_API

PyObject *
_PyFunctionCode_FastCall(
    PyCodeObject *co,
    PyObject *const *args,
    Py_ssize_t nargsf,
    PyObject *globals,
    PyObject *name,
    PyObject *qualname);

PyAPI_FUNC(PyObject *) _PyFunction_FastCallDict(
    PyObject *func,
    PyObject *const *args,
    Py_ssize_t nargs,
    PyObject *kwargs);

PyAPI_FUNC(PyObject *) _PyFunction_Vectorcall(
    PyObject *func,
    PyObject *const *stack,
    size_t nargsf,
    PyObject *kwnames);

/* TODO(mpage) - Remove this and fetch builtins directly from tstate */
PyAPI_FUNC(PyObject *) _PyFunction_GetBuiltins(PyFunctionObject *func);

PyAPI_FUNC(int) _PyFunction_InitSwitchboard(void);
/* Returns a borrowed reference */
PyAPI_FUNC(PyObject *) _PyFunction_GetSwitchboard(void);
PyAPI_FUNC(void) _PyFunction_ClearSwitchboard(void);

PyAPI_FUNC(PyObject *) _PyStaticMethod_GetFunc(PyObject *method);
PyAPI_FUNC(PyObject *) _PyClassMethod_GetFunc(PyObject *method);

int _PyFunction_ClearFreeList(void);

#endif

/* Macros for direct access to these values. Type checks are *not*
   done, so use with care. */
#define PyFunction_GET_CODE(func) \
        (((PyFunctionObject *)func) -> func_code)
#define PyFunction_GET_GLOBALS(func) \
        (((PyFunctionObject *)func) -> func_globals)
#define PyFunction_GET_MODULE(func) \
        (((PyFunctionObject *)func) -> func_module)
#define PyFunction_GET_DEFAULTS(func) \
        (((PyFunctionObject *)func) -> func_defaults)
#define PyFunction_GET_KW_DEFAULTS(func) \
        (((PyFunctionObject *)func) -> func_kwdefaults)
#define PyFunction_GET_CLOSURE(func) \
        (((PyFunctionObject *)func) -> func_closure)
#define PyFunction_GET_ANNOTATIONS(func) \
        (((PyFunctionObject *)func) -> func_annotations)

/* The classmethod and staticmethod types lives here, too */
PyAPI_DATA(PyTypeObject) PyClassMethod_Type;
PyAPI_DATA(PyTypeObject) PyStaticMethod_Type;

#define PyClassMethod_Check(op) (Py_TYPE(op) == &PyClassMethod_Type)
#define _PyStaticMethod_Check(op) (Py_TYPE(op) == &PyStaticMethod_Type)

PyAPI_FUNC(PyObject *) PyClassMethod_New(PyObject *);
PyAPI_FUNC(PyObject *) PyStaticMethod_New(PyObject *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_FUNCOBJECT_H */
#endif /* Py_LIMITED_API */
