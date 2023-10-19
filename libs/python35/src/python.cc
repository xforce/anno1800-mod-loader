#include "Python.h"

#include "mod_manager.h"

#include "spdlog/spdlog.h"

#include <string>

static bool     scripts_done = false;
void RunModScripts()
{
    if (scripts_done)
        return;

    // Check if it is possible to execute console.startScript command
    scripts_done = PyRun_SimpleString("hasattr(console, 'startScript') and callable(getattr(console, 'startScript'))") == 0;
    if (!scripts_done) return;

    const auto& python_scripts = ModManager::instance().GetPythonScripts();
    spdlog::info("Loading python script");
    for (const auto& str : python_scripts) {
        spdlog::info("Python: {}", str);
        int err = PyRun_SimpleString(str.c_str());
        if (err < 0) spdlog::info("... failed", str);
    }
}

extern "C" void Py_Initialize_S()
{
    spdlog::info("Python initialized");
    Py_Initialize();

    scripts_done = false;
}

extern "C" PyObject* PyRun_StringFlags_S(const char* str, int start, PyObject* globals,
                                         PyObject* locals, PyCompilerFlags* flags)
{
    RunModScripts();
    return PyRun_StringFlags(str, start, globals, locals, flags);
}

extern "C" PyObject* PyObject_CallObject_S(PyObject* callable, PyObject* args)
{
    RunModScripts();
    return PyObject_CallObject(callable, args);
}

extern "C" PyObject* PyObject_Call_S(PyObject* callable, PyObject* args, PyObject* kwargs)
{
    RunModScripts();
    return PyObject_Call(callable, args, kwargs);
}

extern "C" PyObject* PyEval_CallFunction_S(PyObject* callable, const char* format, ...)
{
    RunModScripts();

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
    RunModScripts();

    va_list list;
    va_start(list, format);
    auto* py_args = Py_VaBuildValue(format, list);
    va_end(list);
    auto* ret = PyObject_CallObject(callable, py_args);
    Py_XDECREF(py_args);
    return ret;
}
