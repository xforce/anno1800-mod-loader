#include "Python.h"

#include <string>
#include "libs/external-file-loader/include/ModManager.h"
#include "spdlog/spdlog.h"

static bool scripts_done = false;
extern "C" void Py_Initialize_S() {
  // PyImport_AppendInittab("aview", PyInit_aview);
  Py_Initialize();
  // PyImport_ImportModule("aview");
}
void debug_print(std::string str) {
  spdlog::info("Python: {}", str);
}
extern "C" PyObject* PyRun_StringFlags_S(const char* str,
                                         int start,
                                         PyObject* globals,
                                         PyObject* locals,
                                         PyCompilerFlags* flags) {
  std::string meow = str;
  debug_print(meow);
  auto* result = PyRun_StringFlags(meow.c_str(), start, globals, locals, flags);
  if (!scripts_done) {
    scripts_done = true;
    std::vector<std::string> pyScripts = GetPyScriptsVector();
    for (std::string str : pyScripts) {
      debug_print(str);
      PyRun_StringFlags(str.c_str(), start, globals, locals, flags);
    }
  }
  return result;
}

extern "C" PyObject* PyObject_CallObject_S(PyObject* callable, PyObject* args) {
  return PyObject_CallObject(callable, args);
}

extern "C" PyObject* PyObject_Call_S(PyObject* callable,
                                     PyObject* args,
                                     PyObject* kwargs) {
  return PyObject_Call(callable, args, kwargs);
}

extern "C" PyObject* PyEval_CallFunction_S(PyObject* callable,
                                           const char* format,
                                           ...) {
  va_list list;
  va_start(list, format);
  auto* py_args = Py_VaBuildValue(format, list);
  va_end(list);
  auto* ret = PyObject_CallObject(callable, py_args);
  Py_XDECREF(py_args);
  return ret;
}

extern "C" PyObject* PyObject_CallFunction_S(PyObject* callable,
                                             const char* format,
                                             ...) {
  va_list list;
  va_start(list, format);
  auto* py_args = Py_VaBuildValue(format, list);
  va_end(list);
  auto* ret = PyObject_CallObject(callable, py_args);
  Py_XDECREF(py_args);
  return ret;
}
