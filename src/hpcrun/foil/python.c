// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "python.h"

#include <dlfcn.h>

static struct Dispatch {
  void (*Py_IncRef)(PyObject*);
  void (*Py_DecRef)(PyObject*);
  const char* (*PyEval_GetFuncName)(PyObject*);
  void (*PyEval_SetProfile)(Py_tracefunc, PyObject*);
  PyFrameObject* (*PyFrame_GetBack)(PyFrameObject*);
  PyCodeObject* (*PyFrame_GetCode)(PyFrameObject*);
  int (*PyFrame_GetLineNumber)(PyFrameObject*);
  int (*PySys_AddAuditHook)(Py_AuditHookFunction, void*);
  const char* (*PyUnicode_AsUTF8)(PyObject*);
} dispatch;

// Fallbacks for functions added in Python 3.9
#if PY_VERSION_HEX >= 0x030800F0 && PY_VERSION_HEX < 0x030900F0
#define FALLBACK_PY_3_9

static PyFrameObject* fallback_PyFrame_GetBack(PyFrameObject* frame) {
  dispatch.Py_IncRef((PyObject*)frame->f_back);
  return frame->f_back;
}

static PyCodeObject* fallback_PyFrame_GetCode(PyFrameObject* frame) {
  dispatch.Py_IncRef((PyObject*)frame->f_code);
  return frame->f_code;
}
#endif

bool f_identify_libpython(const char* path) {
  if (dispatch.PySys_AddAuditHook != NULL) {
    return false;
  }
  void* libpy = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
  if (dlsym(libpy, "PySys_AddAuditHook") == NULL) {
    dlclose(libpy);
    return false;
  }
  dispatch = (struct Dispatch){
      .Py_IncRef = dlsym(libpy, "Py_IncRef"),
      .Py_DecRef = dlsym(libpy, "Py_DecRef"),
      .PyEval_GetFuncName = dlsym(libpy, "PyEval_GetFuncName"),
      .PyEval_SetProfile = dlsym(libpy, "PyEval_SetProfile"),
#ifdef FALLBACK_PY_3_9
      .PyFrame_GetBack = &fallback_PyFrame_GetBack,
      .PyFrame_GetCode = &fallback_PyFrame_GetCode,
#else
      .PyFrame_GetBack = dlsym(libpy, "PyFrame_GetBack"),
      .PyFrame_GetCode = dlsym(libpy, "PyFrame_GetCode"),
#endif
      .PyFrame_GetLineNumber = dlsym(libpy, "PyFrame_GetLineNumber"),
      .PySys_AddAuditHook = dlsym(libpy, "PySys_AddAuditHook"),
      .PyUnicode_AsUTF8 = dlsym(libpy, "PyUnicode_AsUTF8"),
  };
  return true;
}

void f_Py_IncRef(PyObject* obj) { return dispatch.Py_IncRef(obj); }

void f_Py_DecRef(PyObject* obj) { return dispatch.Py_DecRef(obj); }

const char* f_PyEval_GetFuncName(PyObject* func) {
  return dispatch.PyEval_GetFuncName(func);
}

void f_PyEval_SetProfile(Py_tracefunc func, PyObject* arg) {
  return dispatch.PyEval_SetProfile(func, arg);
}

PyFrameObject* f_PyFrame_GetBack(PyFrameObject* frame) {
  return dispatch.PyFrame_GetBack(frame);
}

PyCodeObject* f_PyFrame_GetCode(PyFrameObject* frame) {
  return dispatch.PyFrame_GetCode(frame);
}

int f_PyFrame_GetLineNumber(PyFrameObject* frame) {
  return dispatch.PyFrame_GetLineNumber(frame);
}

int f_PySys_AddAuditHook(Py_AuditHookFunction func, void* arg) {
  return dispatch.PySys_AddAuditHook(func, arg);
}

const char* f_PyUnicode_AsUTF8(PyObject* str) { return dispatch.PyUnicode_AsUTF8(str); }
