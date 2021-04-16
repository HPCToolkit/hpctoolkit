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

#include "loadmap.h"
#include "messages/messages.h"
#include "thread_data.h"
#include "safe-sampling.h"

#include <libelf.h>
#include <gelf.h>

#include <fcntl.h>
#include <sys/mman.h>

#include <assert.h>
#include <dlfcn.h>
#include <Python.h>
#include <frameobject.h>
#include <stddef.h>

// dl* macros for using Python functions. We don't link Python so this is needed

#define DL(NAME) dl_##NAME

#define PYFUNCS(F) \
  F(PyEval_GetFuncName) \
  F(PyEval_SetProfile) \
  F(PyFrame_GetBack) \
  F(PyFrame_GetCode) \
  F(PyFrame_GetLineNumber) \
  F(PySys_AddAuditHook) \
  F(PyUnicode_AsUTF8) \
// END PYFUNCS

#define DL_DEF(NAME) static typeof(NAME)* dl_##NAME = NULL;
PYFUNCS(DL_DEF)
#undef DL_DEF

// -----------------------------
// Python unwinder
// -----------------------------

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
static bool python_unwind(logical_region_t* region, void** store,
    unsigned int index, logical_frame_t* in_lframe, frame_t* frame) {
  logical_python_region_t* state = &region->specific.python;
  logical_python_frame_t* lframe = &in_lframe->python;
  if(state->cfunc != NULL) {
    if(index == 0) {
      ETMSG(LOGICAL_CTX_PYTHON, "Exited Python through C function %s",
            DL(PyEval_GetFuncName)(state->cfunc));
      if(lframe->lm == 0) lframe->lm = get_lm(DL(PyEval_GetFuncName)(state->cfunc), NULL);
      frame->ip_norm.lm_id = lframe->lm;
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
  if(lframe->lm == 0) lframe->lm = get_lm(DL(PyUnicode_AsUTF8)(code->co_name), DL(PyUnicode_AsUTF8)(code->co_filename));
  frame->ip_norm.lm_id = lframe->lm;
  frame->ip_norm.lm_ip = DL(PyFrame_GetLineNumber)(pyframe);
  *store = DL(PyFrame_GetBack)(pyframe);
  return *store != state->caller;
}

// -----------------------------
// Python integration hooks
// -----------------------------

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
      top = hpcrun_logical_stack_push(lstack, &(logical_region_t){
        .generator = python_unwind, .specific = {.python = {
          .caller = DL(PyFrame_GetBack)(frame), .frame = frame, .cfunc = NULL,
        }},
        .expected = 1,  // This should be a top-level frame
        .enter = STACK_MARK_2(), .exit = NULL,
      });
    } else {
      // Update the top region with the new frame
      top->specific.python.frame = frame;
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
    top->specific.python.cfunc = arg;
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
    top->specific.python.cfunc = NULL;
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
    if(prevframe == top->specific.python.caller) {
      // We are exiting a Python region. Pop it from the stack.
      hpcrun_logical_substack_settop(lstack, hpcrun_logical_stack_top(lstack), 0);
      hpcrun_logical_stack_pop(lstack, 1);
    } else {
      // Update the top region with the new frame
      top->specific.python.frame = prevframe;
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

// Search for a particular symbol in an ELF dynamic section
// Copied from the module-ignore-map code
static bool has_symbol(Elf *e, GElf_Shdr* secHead, Elf_Scn *section, const char* target) {
  Elf_Data *data;
  char *symName;
  uint64_t count;
  GElf_Sym curSym;
  uint64_t ii,symType, symBind;
  // char *marmite;

  data = elf_getdata(section, NULL);           // use it to get the data
  if (data == NULL || secHead->sh_entsize == 0) return -1;
  count = (secHead->sh_size)/(secHead->sh_entsize);
  for (ii=0; ii<count; ii++) {
    gelf_getsym(data, ii, &curSym);
    symName = elf_strptr(e, secHead->sh_link, curSym.st_name);
    symType = GELF_ST_TYPE(curSym.st_info);
    symBind = GELF_ST_BIND(curSym.st_info);

    // the .dynsym section can contain undefined symbols that represent imported symbols.
    // We need to find functions defined in the module.
    if ( (symType == STT_FUNC) && (symBind == STB_GLOBAL) && (curSym.st_value != 0)) {
      if(strcmp(symName, target) == 0) {
        return true;
      }
    }
	}
  return false;
}

static void python_notify_mapped(load_module_t* lm) {
  // Determine whether this is a Python 3.8 or higher
  // Copied from the module-ignore-map code
  {
    int fd = open (lm->name, O_RDONLY);
    if (fd < 0) return;
    struct stat stat;
    if (fstat (fd, &stat) < 0) {
      close(fd);
      return;
    }

    char* buffer = (char*) mmap (NULL, stat.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (buffer == NULL) {
      close(fd);
      return;
    }
    elf_version(EV_CURRENT);
    Elf *elf = elf_memory(buffer, stat.st_size);
    Elf_Scn *scn = NULL;
    GElf_Shdr secHead;

    bool matches = false;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
      gelf_getshdr(scn, &secHead);
      // Only search .dynsym section
      if (secHead.sh_type != SHT_DYNSYM) continue;
      if (has_symbol(elf, &secHead, scn, "PySys_AddAuditHook")) {
        matches = true;
        break;
      }
    }
    munmap(buffer, stat.st_size);
    close(fd);

    if(!matches) return;
  }

  // Pop open the module and nab out the symbols we need
  void* libpy = dlopen(lm->name, RTLD_LAZY | RTLD_NOLOAD);
  if(libpy == NULL) {
    // This might be the main executable, so look there too
    libpy = dlopen(NULL, RTLD_LAZY | RTLD_NOLOAD);
    if(dlsym(libpy, "PySys_AddAuditHook") == NULL)
      libpy = NULL;
  }
  if(libpy == NULL) {
    EEMSG("WARNING: Python found but failed to dlopen, Python unwinding not enabled: %s", lm->name);
    return;
  }
  #define SAVE(NAME) \
    dl_##NAME = dlsym(libpy, #NAME); \
    if(dl_##NAME == NULL) { \
      EEMSG("WARNING: Python does not have symbol " #NAME ", Python unwinding not enabled"); \
      return; \
    }
  PYFUNCS(SAVE)
  #undef SAVE

  // If all went well, we can now register our Python audit hook
  if(DL(PySys_AddAuditHook)(python_audit, NULL) != 0)
    EEMSG("Python error while adding audit hook, Python unwinding may not be enabled");
}

static loadmap_notify_t python_loadmap_notify = {
  .map = python_notify_mapped, .unmap = NULL,
};
void hpcrun_logical_python_init() {
  hpcrun_loadmap_notify_register(&python_loadmap_notify);
}
