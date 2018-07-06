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
// Copyright ((c)) 2002-2018, Rice University
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

#define UNW_LOCAL_ONLY

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include <ucontext.h>

#include <include/hpctoolkit-config.h>

//************************************************
// external includes
//************************************************

#include <monitor.h>
#include <libunwind.h>

//************************************************
// local includes
//************************************************

#include <fnbounds/fnbounds_interface.h>
#include <messages/messages.h>
#include <hpcrun/hpcrun_stats.h>
#include <unwind/common/unw-datatypes.h>
#include <unwind/common/unwind.h>
#include <unwind/common/uw_recipe_map.h>
#include <unwind/common/binarytree_uwi.h>
#include <unwind/common/libunw_intervals.h>
#include <utilities/arch/context-pc.h>

//************************************************
// interface functions
//************************************************

// ----------------------------------------------------------
// hpcrun_unw_init
// ----------------------------------------------------------

void
hpcrun_unw_init(void)
{
  uw_recipe_map_init();
}


// ----------------------------------------------------------
// hpcrun_unw_get_ip_reg
// ----------------------------------------------------------

int
hpcrun_unw_get_ip_reg(hpcrun_unw_cursor_t* cursor, void** val)
{
  unw_word_t tmp;
  int rv = unw_get_reg(&(cursor->uc), UNW_REG_IP, &tmp);
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
  return libunw_unw_step(cursor);
}

btuwi_status_t
build_intervals(char  *ins, unsigned int len, unwinder_t uw)
{
  return libunw_build_intervals(ins, len);
}
