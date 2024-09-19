// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_PYTHON_H
#define HPCRUN_FOIL_PYTHON_H

#include <Python.h>
#include <frameobject.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Mark the given filename as a (potential) CPython implementation.
/// This is extremely atypical for foils, but is used for this specific case.
/// TODO: Remove this entire foil and replace with a "push"-style interface.
/// @returns `true` if this was indeed libpython, `false` otherwise.
bool f_identify_libpython(const char* path);

void f_Py_IncRef(PyObject*);
void f_Py_DecRef(PyObject*);
const char* f_PyEval_GetFuncName(PyObject*);
void f_PyEval_SetProfile(Py_tracefunc, PyObject*);
PyFrameObject* f_PyFrame_GetBack(PyFrameObject*);
PyCodeObject* f_PyFrame_GetCode(PyFrameObject*);
int f_PyFrame_GetLineNumber(PyFrameObject*);
int f_PySys_AddAuditHook(Py_AuditHookFunction, void*);
const char* f_PyUnicode_AsUTF8(PyObject*);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_PYTHON_H
