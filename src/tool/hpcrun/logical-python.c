// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#define _GNU_SOURCE 1

#include "logical.h"

#include "messages/messages.h"

#include <dlfcn.h>
#include <Python.h>

#define DL_DEF(NAME) static typeof(NAME)* dl_##NAME = NULL;
#define DL_NOERROR(NAME) ({                                     \
  if(dl_##NAME == NULL) dl_##NAME = dlsym(RTLD_DEFAULT, #NAME); \
  dl_##NAME;                                                    \
})
#define DL(NAME) ({                                             \
  DL_NOERROR(NAME);                                             \
  if(dl_##NAME == NULL) {                                       \
    EMSG("Python dl error: %s", dlerror());                     \
    abort();                                                    \
  }                                                             \
  dl_##NAME;                                                    \
})

DL_DEF(PyEval_SetProfile)
DL_DEF(PyFrame_GetCode)
DL_DEF(PyFrame_GetLineNumber)
DL_DEF(PySys_AddAuditHook)
DL_DEF(PyEval_GetFuncName)
DL_DEF(PyUnicode_AsUTF8)

static int python_profile(PyObject* ud, PyFrameObject* frame, int what, PyObject* arg) {
  const char* whatstr = "(unknown)";
  switch(what) {
  #define CASE(N) case PyTrace_##N: whatstr = #N; break
  CASE(CALL);
  CASE(EXCEPTION);
  CASE(LINE);
  CASE(RETURN);
  CASE(C_CALL);
  CASE(C_EXCEPTION);
  CASE(C_RETURN);
  CASE(OPCODE);
  #undef CASE
  }

  PyCodeObject* code = DL(PyFrame_GetCode)(frame);
  const char* filename = DL(PyUnicode_AsUTF8)(code->co_filename);
  const char* codename = DL(PyUnicode_AsUTF8)(code->co_name);
  int line = DL(PyFrame_GetLineNumber)(frame);
  ETMSG(LOGICAL_CTX, "%s at %s:%d: %s %s", whatstr, filename, line, codename,
    arg == NULL ? "(null)" :
      arg == Py_None ? "(None)" : DL(PyEval_GetFuncName)(arg));

  return 0;
}

static int python_audit(const char* event, PyObject* args, void* ud) {
  if(strcmp(event, "sys.setprofile") != 0)
    DL(PyEval_SetProfile)(python_profile, NULL);
  return 0;
}

__attribute__((constructor))
static void python_hook_init() {
  // If there is no Python for us, don't bother
  if(DL_NOERROR(PySys_AddAuditHook) == NULL) return;

  // Try to add a hook for when the interpreter is initialized
  if(DL(PySys_AddAuditHook)(python_audit, NULL) != 0) {
    EMSG("Python error while adding audit hook!");
    return;
  }
}
