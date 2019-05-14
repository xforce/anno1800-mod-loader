#include "Python.h"

#include <string>

extern "C" void Py_Initialize_S()
{
    // PyImport_AppendInittab("aview", PyInit_aview);
    Py_Initialize();
    // PyImport_ImportModule("aview");
}

extern "C" PyObject* PyRun_StringFlags_S(const char* str, int start, PyObject* globals,
                                         PyObject* locals, PyCompilerFlags* flags)
{
    std::string meow   = str;
    auto*       result = PyRun_StringFlags(meow.c_str(), start, globals, locals, flags);
    return result;
}

extern "C" PyObject* PyObject_CallObject_S(PyObject* callable, PyObject* args)
{
    return PyObject_CallObject(callable, args);
}

extern "C" PyObject* PyObject_Call_S(PyObject* callable, PyObject* args, PyObject* kwargs)
{
    return PyObject_Call(callable, args, kwargs);
}

extern "C" PyObject* PyEval_CallFunction_S(PyObject* callable, const char* format, ...)
{
    va_list list;
    va_start(list, format);
    auto* py_args = Py_VaBuildValue(format, list);
    va_end(list);
    auto* ret = PyObject_CallObject(callable, py_args);
    Py_XDECREF(py_args);
    return ret;
}

extern "C" PyObject* PyObject_CallFunction_S(PyObject* callable, const char* format, ...)
{
    va_list list;
    va_start(list, format);
    auto* py_args = Py_VaBuildValue(format, list);
    va_end(list);
    auto* ret = PyObject_CallObject(callable, py_args);
    Py_XDECREF(py_args);
    return ret;
}