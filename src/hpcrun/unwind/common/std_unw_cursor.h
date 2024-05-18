// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef UNWIND_CURSOR_H
#define UNWIND_CURSOR_H

//************************* System Include Files ****************************

#include <inttypes.h>
#include <ucontext.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

//*************************** User Include Files ****************************

#include "fence_enum.h"
#include "../../utilities/ip-normalized.h"

//*************************** Forward Declarations **************************

// HPC_UNW_LITE: It is not safe to have a pointer to the interval
// since we cannot use dynamic storage.
#if (HPC_UNW_LITE)

   // there should probably have a check to ensure this is big enough
   typedef struct { char data[128]; } unw_interval_opaque_t;
#  define UNW_CURSOR_INTERVAL_t unw_interval_opaque_t

#else

#include "unwindr_info.h"
#define UNW_CURSOR_INTERVAL_t bitree_uwi_t*

#endif

enum libunw_state {
  LIBUNW_UNAVAIL,
  LIBUNW_READY,
};

typedef struct hpcrun_unw_cursor_t {

  // ------------------------------------------------------------
  // common state
  // ------------------------------------------------------------
  void *pc_unnorm; // only place where un-normalized pc exists
  void **bp;       // maintained only on x86_64
  void **sp;
  void *ra;

  void *ra_loc;    // return address location (for trampolines)

  fence_enum_t fence; // which fence stopped an unwind
  unwindr_info_t unwr_info; // unwind recipe info
  ip_normalized_t the_function; // (normalized) ip for function

  //NOTE: will fail if HPC_UWN_LITE defined
  ip_normalized_t pc_norm;

  // ------------------------------------------------------------
  // unwind-provider-specific state
  // ------------------------------------------------------------
  int32_t flags:30;
  enum libunw_state libunw_status:2;

#ifdef HOST_CPU_PPC
  ucontext_t *ctxt; // needed for register-based unwinding
#endif

  unw_cursor_t uc;
} hpcrun_unw_cursor_t;


//***************************************************************************

#endif
