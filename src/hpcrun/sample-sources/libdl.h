// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   libdl.h
//
// Purpose:
//   simple wrappers that facilitate using dlopen and dlsym to dynamically
//   bind symbols for use by hpcrun sample sources.
//
//***************************************************************************

#ifndef _HPCTOOLKIT_LIBDL_H_
#define _HPCTOOLKIT_LIBDL_H_

//*****************************************************************************
// system include files
//*****************************************************************************

#include <dlfcn.h>



//*****************************************************************************
// local include files
//*****************************************************************************

#include "../libmonitor/monitor.h"



//*****************************************************************************
// macros
//*****************************************************************************


#define DYNAMIC_BINDING_STATUS_UNINIT -1
#define DYNAMIC_BINDING_STATUS_ERROR  -1
#define DYNAMIC_BINDING_STATUS_OK      0

#define DYN_FN_NAME(f) f ## _fn

#define CHK_DLOPEN(h, lib, flags)                                       \
  void* h = dlopen(lib, flags);                                         \
  if (!h) {                                                             \
    return DYNAMIC_BINDING_STATUS_ERROR;                                \
  }

#define CHK_DLSYM(h, fn) {                                              \
    dlerror();                                                          \
    DYN_FN_NAME(fn) = dlsym(h, #fn);                                    \
    if (DYN_FN_NAME(fn) == 0) {                                         \
      return DYNAMIC_BINDING_STATUS_ERROR;                              \
    }                                                                   \
  }

#endif
