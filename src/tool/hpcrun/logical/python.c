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
// Copyright ((c)) 2002-2023, Rice University
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

// -----------------------------
// Local variables
// -----------------------------

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

static logical_metadata_store_t python_metastore;

// -----------------------------
// Python unwinder
// -----------------------------

static bool python_unwind(logical_region_t* region, void** store,
    unsigned int index, logical_frame_t* in_lframe, frame_t* frame) {
  logical_python_region_t* state = &region->specific.python;
  if (in_lframe == NULL) {
    // opportunity for enhancement to handle the case when logical
    // frames and python frames don't match. for now, give up,
    // terminating the unwind by returning false.
    return false;
  }
  logical_python_frame_t* lframe = &in_lframe->python;

  if(state->cfunc != NULL) {
    // We already exited Python via a call to a C function, so the topmost
    // logical frame should be the C function (from Python's perspective).
    // Emit that frame here and adjust index so the rest of the code is general.
    if(index == 0) {
      TMSG(LOGICAL_CTX_PYTHON, "Exited Python through C function %s",
           DL(PyEval_GetFuncName)(state->cfunc));
      if(lframe->fid == 0)
        lframe->fid = hpcrun_logical_metadata_fid(&python_metastore, DL(PyEval_GetFuncName)(state->cfunc), LOGICAL_MANGLING_NONE, NULL, 0);
      frame->ip_norm = hpcrun_logical_metadata_ipnorm(&python_metastore, lframe->fid, 0);
      return true;
    }
    index--;
  }

  // Initialize *store to the topmost PyFrameObject
  if(index == 0) *store = state->frame;

  // Decipher the PyFrameObject to generate the final logical frame
  PyFrameObject* pyframe = *store;
  PyCodeObject* code = DL(PyFrame_GetCode)(pyframe);
  Py_DECREF(code);
  TMSG(LOGICAL_CTX_PYTHON, "Unwinding Python frame %p: %s:%d in function %s (starting at line %d)",
       pyframe, DL(PyUnicode_AsUTF8)(code->co_filename),
       DL(PyFrame_GetLineNumber)(pyframe), DL(PyUnicode_AsUTF8)(code->co_name),
       code->co_firstlineno);
  if(lframe->fid == 0 || lframe->code != code) {
    const char* name = DL(PyUnicode_AsUTF8)(code->co_name);
    if(strcmp(name, "<module>") == 0)
      name = NULL;  // Special case, the main module should just have its filename
    lframe->fid = hpcrun_logical_metadata_fid(&python_metastore,
      name, LOGICAL_MANGLING_NONE, DL(PyUnicode_AsUTF8)(code->co_filename), code->co_firstlineno);
    lframe->code = code;
    TMSG(LOGICAL_CTX_PYTHON, "Registered the above as Python fid #%x", lframe->fid);
  }
  frame->ip_norm = hpcrun_logical_metadata_ipnorm(&python_metastore, lframe->fid, DL(PyFrame_GetLineNumber)(pyframe));

  // Fetch the next frame in the unwind and save that in *store for next time
  PyFrameObject* prevframe = DL(PyFrame_GetBack)(pyframe);
  Py_XDECREF(prevframe);
  if(ENABLED(LOGICAL_CTX_PYTHON) && prevframe == state->caller && index+1 < region->expected)
    TMSG(LOGICAL_CTX_PYTHON, "Python appears to be missing some frames, got %d of %d (caller = %p)", index+1, region->expected, state->caller);
  *store = prevframe;

  // Stop whenever the next unwound frame is not part of this logical region
  // PyFrame_GetBack returns NULL and state->caller is NULL for the bottommost
  // PyFrameObject/logical_region_t, so this is a generic comparison.
  return prevframe != state->caller;
}

// -----------------------------
// Python frame fixup routines
// -----------------------------

static const frame_t* python_afterexit(logical_region_t* reg,
    const frame_t* cur, const frame_t* top) {
  if(top->ip_norm.lm_id == reg->specific.python.lm) return top;
  const frame_t* precur = cur;
  while(cur->ip_norm.lm_id == reg->specific.python.lm && cur != top)
    precur = cur, cur--;
  return precur;
}

// -----------------------------
// Python integration hooks
// -----------------------------

// NOTE: Only used for producing nicer debug log output.
static const char* name_for(uint16_t lm_id) {
  const load_module_t* lm = hpcrun_loadmap_findById(lm_id);
  return lm == NULL ? "<not found>" : lm->name;
}

static int python_profile(PyObject* ud, PyFrameObject* frame, int what, PyObject* arg) {
  int safe_ok = hpcrun_safe_enter();
  if (!safe_ok)
    hpcrun_terminate();  // Python called by init code???
  thread_data_t* td = hpcrun_get_thread_data();
  logical_region_stack_t* lstack = &td->logical_regs;
  switch(what) {
  case PyTrace_CALL: {  // Some Python function was called
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    IF_ENABLED(LOGICAL_CTX_PYTHON) {
      PyCodeObject* code = DL(PyFrame_GetCode)(frame);
      Py_DECREF(code);
      TMSG(LOGICAL_CTX_PYTHON, "[%d -> %d] call of Python function %s (%s:%d), now at frame = %p",
           top == NULL ? -1 : top->expected, top == NULL ? 1 : top->expected+1,
           DL(PyUnicode_AsUTF8)(code->co_name),
           DL(PyUnicode_AsUTF8)(code->co_filename),
           DL(PyFrame_GetLineNumber)(frame), frame);
    }
    if(top == NULL || top->exit_len > 0) {
      // We are (re)entering Python from C. Push a brand new logical region
      logical_region_t reg = {
        .generator = python_unwind, .specific = {.python = {
          .lm = 0, .caller = DL(PyFrame_GetBack)(frame),
          .frame = frame, .cfunc = NULL,
        }},
        .expected = 1,
        .beforeenter = {}, .exit = {}, .exit_len = 0, .afterexit = python_afterexit,
      };
      Py_XDECREF(reg.specific.python.caller);

      // Unwind out through libpython.so to identify the appropriate C caller
      hpcrun_logical_frame_cursor(&reg.beforeenter, 1);
      reg.specific.python.lm = reg.beforeenter.pc_norm.lm_id;
      TMSG(LOGICAL_CTX_PYTHON, "Fixing beforeenter be out of libpython.so, start sp = %p  ip = %s +%p",
        reg.beforeenter.sp, name_for(reg.beforeenter.pc_norm.lm_id), reg.beforeenter.pc_norm.lm_ip);
      while(reg.beforeenter.pc_norm.lm_id == reg.specific.python.lm) {
        if(hpcrun_unw_step(&reg.beforeenter) <= STEP_STOP)
          break;
        TMSG(LOGICAL_CTX_PYTHON, "  stepped to sp = %p  ip = %s +%p",
          reg.beforeenter.sp, name_for(reg.beforeenter.pc_norm.lm_id), reg.beforeenter.pc_norm.lm_ip);
      }
      TMSG(LOGICAL_CTX_PYTHON, "Exited at sp = %p  ip = %s +%p",
          reg.beforeenter.sp, name_for(reg.beforeenter.pc_norm.lm_id), reg.beforeenter.pc_norm.lm_ip);

      top = hpcrun_logical_stack_push(lstack, &reg);
    } else {
      // Python is calling Python. Update the topmost frame in the region.
      top->specific.python.frame = frame;
      top->expected++;
    }

    // Push a blank frame onto the substack for caching bits during the unwind.
    hpcrun_logical_substack_push(lstack, top, &(logical_frame_t){.python={0, NULL}});
    break;
  }
  case PyTrace_C_CALL: {  // Python is calling a C function
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    if (top == NULL) {
      TMSG(LOGICAL_CTX_PYTHON, "[? -> ?] ignoring attempt to call into C via function %s from frame = %p: no logical stack",
           DL(PyEval_GetFuncName)(arg), frame);
      break;
    }
    TMSG(LOGICAL_CTX_PYTHON, "[%d -> %d] call into C via function %s from frame = %p",
         top->expected, top->expected+1, DL(PyEval_GetFuncName)(arg), frame);

    // Update the top region to record that we have exited Python. Assume there
    // is a stable physical frame within the top 4 callers of this callback.
    top->specific.python.cfunc = arg;
    top->expected++;
    top->exit_len = 4;
    for(size_t i = 0; i < top->exit_len; ++i)
      top->exit[i] = hpcrun_logical_frame_sp(i + 1);

    // Push a blank frame onto the substack for caching bits during the unwind.
    hpcrun_logical_substack_push(lstack, top, &(logical_frame_t){.python={0, NULL}});
    break;
  }
  case PyTrace_C_RETURN: {  // A C function is returning, back into Python
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    if (top == NULL) {
      TMSG(LOGICAL_CTX_PYTHON, "[? -> ?] ignoring attempt to return from C frame into Python frame = %p: no logical stack",
           frame);
      break;
    }
    TMSG(LOGICAL_CTX_PYTHON, "[%d -> %d] return from C into Python frame = %p",
         top->expected, top->expected-1, frame);

    // Update the top region to record that we have re-entered Python
    top->specific.python.cfunc = NULL;
    top->expected--;
    top->exit_len = 0;

    // Pop the topmost frame from the substack, we don't need it anymore.
    hpcrun_logical_substack_pop(lstack, top, 1);
    break;
  }
  case PyTrace_RETURN: {  // Python is returning from a Python function.
    logical_region_t* top = hpcrun_logical_stack_top(lstack);
    if (top == NULL) {
      TMSG(LOGICAL_CTX_PYTHON, "[? -> ?] ignoring attempt to return from Python frame = %p: no logical stack",
           frame);
      break;
    }

    // Fetch the frame we are returning into
    PyFrameObject* prevframe = DL(PyFrame_GetBack)(frame);
    Py_XDECREF(prevframe);
    TMSG(LOGICAL_CTX_PYTHON, "[%d -> %d] (about to) return from Python frame = %p, now at frame %p",
         top->expected, top->expected-1, frame, prevframe);

    if(prevframe == top->specific.python.caller) {
      // We are actually returning out of a Python function called from C.
      // Pop the topmost logical region and pull out.
      hpcrun_logical_substack_settop(lstack, top, 0);
      hpcrun_logical_stack_pop(lstack, 1);
    } else {
      // Update the top region with the new frame, and pop the unneeded frame
      top->specific.python.frame = prevframe;
      top->expected--;
      hpcrun_logical_substack_pop(lstack, top, 1);
    }
    break;
  }
  case PyTrace_C_EXCEPTION:
    // We seem to get a C_RETURN right after this, so we don't need to do
    // anything special here.
    break;
  case PyTrace_EXCEPTION:
  case PyTrace_LINE:
  case PyTrace_OPCODE:
    assert(false && "Invalid what for Python profile function!");
    hpcrun_terminate();
  }
  hpcrun_safe_exit();
  return 0;
}

static int python_audit(const char* event, PyObject* args, void* ud) {
  // Set the trace function so we start getting callbacks from Python
  if(strcmp(event, "sys.setprofile") != 0)
    DL(PyEval_SetProfile)(python_profile, NULL);

  // We want our samples to be recorded in Python form, so make sure the
  // backtrace finalizer to make that happen is registered.
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
  hpcrun_logical_metadata_register(&python_metastore, "python");
  hpcrun_loadmap_notify_register(&python_loadmap_notify);
}
