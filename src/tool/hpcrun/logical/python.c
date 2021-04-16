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

#include "logical/common.h"

#include "messages/messages.h"
#include "thread_data.h"
#include "safe-sampling.h"

#include <assert.h>
#include <dlfcn.h>
#include <Python.h>
#include <frameobject.h>
#include <stddef.h>

// dl* macros for using Python functions. We don't link Python so this is needed

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

DL_DEF(PyEval_GetFuncName)
DL_DEF(PyEval_SetProfile)
DL_DEF(PyFrame_GetBack)
DL_DEF(PyFrame_GetCode)
DL_DEF(PyFrame_GetLineNumber)
DL_DEF(PySys_AddAuditHook)
DL_DEF(PyUnicode_AsUTF8)

// Python unwinder

typedef struct python_unwind_state_t {
  PyFrameObject* caller;  // Latest Python frame NOT to report (or NULL)
  PyFrameObject* frame;  // Latest Python frame
  PyObject* cfunc;  // If not NULL, the C function used to exit Python
} python_unwind_state_t;

static uint16_t get_lm(const char* funcname, const char* filename) {
  char name[4096];
  strcpy(name, "$PY$");
  strcat(name, funcname);
  if(filename != NULL) {
    strcat(name, "$FILE$");
    strcat(name, filename);
  }

  load_module_t* lm = hpcrun_loadmap_findByName(name);
  if(lm != NULL) return lm->id;
  return hpcrun_loadModule_add(name);
}

static_assert(UINTPTR_MAX >= UINT16_MAX, "Is this an 8-bit machine?");
static bool python_unwind(void* vp_state, void** store, unsigned int index, uintptr_t* lframe, frame_t* frame) {
  python_unwind_state_t* state = vp_state;
  if(state->cfunc != NULL) {
    if(index == 0) {
      ETMSG(LOGICAL_CTX_PYTHON, "Exited Python through C function %s",
            DL(PyEval_GetFuncName)(state->cfunc));
      if(*lframe == 0) *lframe = get_lm(DL(PyEval_GetFuncName)(state->cfunc), NULL);
      frame->ip_norm.lm_id = *lframe;
      frame->ip_norm.lm_ip = 0;
      return true;
    }
    index--;
  }
  if(index == 0) *store = state->frame;

  PyFrameObject* pyframe = *store;
  PyCodeObject* code = DL(PyFrame_GetCode)(pyframe);
  ETMSG(LOGICAL_CTX_PYTHON, "Unwinding Python frame: %s:%d in function %s",
        DL(PyUnicode_AsUTF8)(code->co_filename), DL(PyFrame_GetLineNumber)(pyframe),
        DL(PyUnicode_AsUTF8)(code->co_name));
  if(*lframe == 0) *lframe = get_lm(DL(PyUnicode_AsUTF8)(code->co_name), DL(PyUnicode_AsUTF8)(code->co_filename));
  frame->ip_norm.lm_id = *lframe;
  frame->ip_norm.lm_ip = DL(PyFrame_GetLineNumber)(pyframe);
  *store = DL(PyFrame_GetBack)(pyframe);
  return *store != state->caller;
}

// Python integration hooks

#define STACK_MARK_2() ({                    \
  ucontext_t context;                        \
  assert(getcontext(&context) == 0);         \
  int steps_taken = 0;                       \
  hpcrun_unw_cursor_t cursor;                \
  hpcrun_unw_init_cursor(&cursor, &context); \
  hpcrun_unw_step(&cursor, &steps_taken);    \
  hpcrun_unw_step(&cursor, &steps_taken);    \
  hpcrun_unw_step(&cursor, &steps_taken);    \
  cursor.sp;                                 \
})

static int python_profile(PyObject* ud, PyFrameObject* frame, int what, PyObject* arg) {
  assert(hpcrun_safe_enter() && "Python called by init code???");
  thread_data_t* td = hpcrun_get_thread_data();
  logical_region_stack_t* lstack = &td->logical;
  switch(what) {
  case PyTrace_CALL: {
    PyCodeObject* code = DL(PyFrame_GetCode)(frame);
    ETMSG(LOGICAL_CTX_PYTHON, "call of Python function %s (%s:%d), now at frame = %p",
      DL(PyUnicode_AsUTF8)(code->co_name),
      DL(PyUnicode_AsUTF8)(code->co_filename),
      DL(PyFrame_GetLineNumber)(frame), frame);
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    if(top == NULL || top->exit != NULL) {
      // We have entered a new Python region for the first time. Push it.
      python_unwind_state_t* unwind = malloc(sizeof *unwind);
      *unwind = (python_unwind_state_t){
        .caller = DL(PyFrame_GetBack)(frame), .frame = frame, .cfunc = NULL,
      };
      top = hpcrun_logical_stack_push(lstack, &(logical_region_t){
        .generator = python_unwind, .generator_arg = unwind,
        .expected = 1,  // This should be a top-level frame
        .enter = STACK_MARK_2(), .exit = NULL,
      });
    } else {
      // Update the top region with the new frame
      ((python_unwind_state_t*)top->generator_arg)->frame = frame;
      top->expected++;
    }
    hpcrun_logical_substack_push(lstack, top, 0);
    break;
  }
  case PyTrace_C_CALL: {
    ETMSG(LOGICAL_CTX_PYTHON, "call into C via function %s from frame = %p",
          DL(PyEval_GetFuncName)(arg), frame);
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    assert(top != NULL && "Python C_CALL without a logical stack???");
    // Update the top region to note that we have exited Python
    ((python_unwind_state_t*)top->generator_arg)->cfunc = arg;
    top->expected++;
    top->exit = STACK_MARK_2();
    hpcrun_logical_substack_push(lstack, top, 0);
    break;
  }
  case PyTrace_C_RETURN: {
    ETMSG(LOGICAL_CTX_PYTHON, "return from C into Python frame = %p", frame);
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    assert(top != NULL && "Python C_RETURN without a logical stack???");
    // Update the top region to note that we have re-entered Python
    ((python_unwind_state_t*)top->generator_arg)->cfunc = NULL;
    top->expected--;
    top->exit = NULL;
    hpcrun_logical_substack_pop(lstack, top, 1);
    break;
  }
  case PyTrace_RETURN: {
    ETMSG(LOGICAL_CTX_PYTHON, "(about to) return from Python frame = %p, now at frame %p", frame,
          DL(PyFrame_GetBack)(frame));
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    assert(top != NULL && "Python RETURN without a logical stack???");
    PyFrameObject* prevframe = DL(PyFrame_GetBack)(frame);
    if(prevframe == ((python_unwind_state_t*)top->generator_arg)->caller) {
      // We are exiting a Python region. Pop it from the stack.
      hpcrun_logical_substack_settop(lstack, hpcrun_logical_stack_top(lstack), 0);
      hpcrun_logical_stack_pop(lstack, 1);
      free(top->generator_arg);
    } else {
      // Update the top region with the new frame
      ((python_unwind_state_t*)top->generator_arg)->frame = prevframe;
      top->expected--;
      hpcrun_logical_substack_pop(lstack, top, 1);
    }
    break;
  }
  case PyTrace_C_EXCEPTION:
    // We seem to get a C_RETURN right after this, so we don't really need to care
    break;
  case PyTrace_EXCEPTION:
  case PyTrace_LINE:
  case PyTrace_OPCODE:
    assert(false && "Invalid what for Python profile function!");
  }
  hpcrun_safe_exit();
  return 0;
}

static int python_audit(const char* event, PyObject* args, void* ud) {
  // We're in! Overwrite the profiler before anything else happens
  if(strcmp(event, "sys.setprofile") != 0)
    DL(PyEval_SetProfile)(python_profile, NULL);
  // Make sure we will be logicalizing backtraces later
  hpcrun_safe_enter();
  hpcrun_logical_register();
  hpcrun_safe_exit();
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
