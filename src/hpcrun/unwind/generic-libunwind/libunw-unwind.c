// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// This software was produced with support in part from the Defense Advanced
// Research Projects Agency (DARPA) through AFRL Contract FA8650-09-C-1915.
// Nothing in this work should be construed as reflecting the official policy or
// position of the Defense Department, the United States government, or
// Rice University.
//
//***************************************************************************
//
// File:
//   libunw-unwind.c
//
// Purpose:
//    Implement the hpcrun unwind primitives using libunwind
//
//        hpcrun_unw_init
//        hpcrun_unw_init_cursor
//        hpcrun_unw_get_ra_loc
//        hpcrun_unw_get_ip_reg
//        hpcrun_unw_step
//        hpcrun_unw_get_ip_unnorm_reg
//
//***************************************************************************

//************************************************
// system includes
//************************************************

#define _GNU_SOURCE

#define UNW_LOCAL_ONLY

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include <ucontext.h>

//************************************************
// external includes
//************************************************

#include "../../libmonitor/monitor.h"
#include <libunwind.h>

//************************************************
// local includes
//************************************************

#include "../../fnbounds/fnbounds_interface.h"
#include "../../messages/messages.h"
#include "../../hpcrun_stats.h"
#include "../common/unw-datatypes.h"
#include "../common/unwind.h"
#include "../common/uw_recipe_map.h"
#include "../common/binarytree_uwi.h"
#include "../common/libunw_intervals.h"
#include "../common/libunwind-interface.h"
#include "../../utilities/arch/context-pc.h"

//************************************************
// interface functions
//************************************************

// ----------------------------------------------------------
// hpcrun_unw_init
// ----------------------------------------------------------

void
hpcrun_unw_init(void)
{
  static bool msg_sent = false;
  if (msg_sent == false) {
    TMSG(NU, "hpcrun_unw_init from libunw_unwind.c" );
    msg_sent = true;
  }
  libunwind_bind();
  uw_recipe_map_init();
}


// ----------------------------------------------------------
// hpcrun_unw_get_ip_reg
// ----------------------------------------------------------

int
hpcrun_unw_get_ip_reg(hpcrun_unw_cursor_t* cursor, void** val)
{
  unw_word_t tmp;
  int rv = libunwind_get_reg(&(cursor->uc), UNW_REG_IP, &tmp);
  *val = (void*) tmp;
  return rv;
}

int
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* cursor, void** val)
{
  *val = cursor->pc_unnorm;
  return 0;
}

int
hpcrun_unw_get_ip_norm_reg(hpcrun_unw_cursor_t* cursor, ip_normalized_t* val)
{
  *val = cursor->pc_norm;
  return 0;
}

// ----------------------------------------------------------
// hpcrun_unw_get_ra_loc (no trampolines yet)
// ----------------------------------------------------------

void*
hpcrun_unw_get_ra_loc(hpcrun_unw_cursor_t* c)
{
  return NULL;
}

// ----------------------------------------------------------
// hpcrun_unw_init_cursor
// ----------------------------------------------------------

// Takes a sigaction() context pointer and makes a libunwind
// unw_cursor_t.  The other fields in hpcrun_unw_cursor_t are unused,
// except for pc_norm and pc_unnorm.
//
void
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context)
{
  libunw_unw_init_cursor(cursor, context);
}

step_state
hpcrun_unw_step(hpcrun_unw_cursor_t* cursor)
{
  static bool msg_sent = false;
  if (msg_sent == false) {
    TMSG(NU, "hpcrun_unw_step from libunw_unwind.c" );
    msg_sent = true;
  }
  return libunw_unw_step(cursor);
}

btuwi_status_t
build_intervals(char  *ins, unsigned int len, unwinder_t uw)
{
  return libunw_build_intervals(ins, len);
}

void
uw_recipe_tostr(void *uwr, char str[], unwinder_t uw)
{
  return libunw_uw_recipe_tostr(uwr, str);
}
