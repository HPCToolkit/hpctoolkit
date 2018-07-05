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

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#include <memory/hpcrun-malloc.h>
#include <hpcrun/hpcrun_stats.h>
#include "x86-unwind-interval.h"

#include <messages/messages.h>

#define STR(s) case s: return #s



/*************************************************************************************
 * forward declarations
 ************************************************************************************/

static const char *ra_status_string(ra_loc l); 
static const char *bp_status_string(bp_loc l);


/*************************************************************************************
 * interface operations 
 ************************************************************************************/

void
suspicious_interval(void *pc) 
{
  TMSG(SUSPICIOUS_INTERVAL,"suspicious interval for pc = %p", pc);
  hpcrun_stats_num_unwind_intervals_suspicious_inc();
}

unwind_interval*
new_ui(char *start, ra_loc ra_status, const x86registers_t *reg)
{
  bitree_uwi_t *u = bitree_uwi_malloc(NATIVE_UNWINDER, sizeof(x86recipe_t));

  // DXN: what is this?
# include "mem_error_gen.h" // **** SPECIAL PURPOSE CODE TO INDUCE MEM FAILURE (conditionally included) ***

  hpcrun_stats_num_unwind_intervals_total_inc();

  uwi_t *uwi =  bitree_uwi_rootval(u);

  uwi->interval.start = (uintptr_t)start;

  x86recipe_t* x86recipe = (x86recipe_t*) uwi->recipe;
  x86recipe->ra_status = ra_status;
  x86recipe->reg = *reg;

  x86recipe->prev_canonical = NULL;
  x86recipe->has_tail_calls = false;

  return u;
}


void
set_ui_canonical(unwind_interval *u, unwind_interval *value)
{
  UWI_RECIPE(u)->prev_canonical = value;
} 

unwind_interval *
fluke_ui(char *loc, unsigned int pos)
{
  bitree_uwi_t *u = bitree_uwi_malloc(NATIVE_UNWINDER, sizeof(x86recipe_t));
  uwi_t *uwi =  bitree_uwi_rootval(u);

  uwi->interval.start = (uintptr_t)loc;
  uwi->interval.end = (uintptr_t)loc;

  x86recipe_t* x86recipe = (x86recipe_t*) uwi->recipe;
  x86recipe->ra_status = RA_SP_RELATIVE;
  x86recipe->reg.sp_ra_pos = pos;
  x86recipe->reg.bp_ra_pos = 0;
  x86recipe->reg.bp_status = 0;
  x86recipe->reg.sp_bp_pos = 0;
  x86recipe->reg.bp_bp_pos = 0;
  x86recipe->prev_canonical = NULL;
  x86recipe->has_tail_calls = false;

  return u;
}

void 
link_ui(unwind_interval *current, unwind_interval *next)
{
  UWI_END_ADDR(current) = UWI_START_ADDR(next);
  bitree_uwi_set_rightsubtree(current, next);
}

static void
dump_ui_str(unwind_interval *u, char *buf, size_t len)
{
  x86recipe_t *xr = UWI_RECIPE(u);
  x86registers_t reg = xr->reg;
  snprintf(buf, len, "UWI: [%8p, %8p) "
           "ra_status=%14s sp_ra_pos=%4d sp_bp_pos=%4d "
           "bp_status=%12s bp_ra_pos=%4d bp_bp_pos=%4d "
           "next=%14p prev_canon=%14p tail_call=%d\n",
           (void *) UWI_START_ADDR(u), (void *) UWI_END_ADDR(u),
           ra_status_string(xr->ra_status), reg.sp_ra_pos, reg.sp_bp_pos,
           bp_status_string(reg.bp_status), reg.bp_ra_pos, reg.bp_bp_pos,
           UWI_NEXT(u), xr->prev_canonical, UWI_RECIPE(u)->has_tail_calls);
}


void
dump_ui_log(unwind_interval *u)
{
  char buf[1000];

  dump_ui_str(u, buf, sizeof(buf));

  EMSG(buf);
}

void
dump_ui(unwind_interval *u, int dump_to_stderr)
{
  if ( ! ENABLED(UNW) ) {
    return;
  }

  char buf[1000];
  dump_ui_str(u, buf, sizeof(buf));

  TMSG(UNW, buf);
  if (dump_to_stderr) { 
    fprintf(stderr, "%s", buf);
    fflush(stderr);
  }
}

void
dump_ui_stderr(unwind_interval *u)
{
  char buf[1000];

  dump_ui_str(u, buf, sizeof(buf));

  EEMSG(buf);
}

void
dump_ui_dbg(unwind_interval *u)
{
  char buf[1000];

  dump_ui_str(u, buf, sizeof(buf));

  fprintf(stderr,"%s", buf);
  fflush(stderr);
}

void
dump_ui_troll(unwind_interval *u)
{
  char buf[1000];

  dump_ui_str(u, buf, sizeof(buf));

  TMSG(TROLL,buf);
}

void
x86recipe_tostr(void* recipe, char str[])
{
  // TODO
  x86recipe_t* x86recipe = (x86recipe_t*)recipe;
  snprintf(str, MAX_RECIPE_STR, "%s%d%s%s",
	  "x86recipe ", x86recipe->reg.sp_ra_pos,
	  ": tail_call = ", x86recipe->has_tail_calls? "true": "false");
}

void
x86recipe_print(void* recipe)
{
  char str[MAX_RECIPE_STR];
  x86recipe_tostr(recipe, str);
  printf("%s", str);
}

/*
 * concrete implementation of the abstract function for printing an abstract
 * unwind recipe specified in uw_recipe.h
 */
void
uw_recipe_tostr(void* recipe, char str[])
{
  x86recipe_tostr((x86recipe_t*)recipe, str);
}

void
uw_recipe_print(void* recipe)
{
  x86recipe_print((x86recipe_t*)recipe);
}


/*************************************************************************************
 * private operations 
 ************************************************************************************/

static const char *
ra_status_string(ra_loc l) 
{
  switch(l) {
   STR(RA_SP_RELATIVE);
   STR(RA_STD_FRAME);
   STR(RA_BP_FRAME);
   STR(RA_REGISTER);
   STR(POISON);
  default:
    assert(0);
  }
  return NULL;
}

static const char *
bp_status_string(bp_loc l)
{
  switch(l){
    STR(BP_UNCHANGED);
    STR(BP_SAVED);
    STR(BP_HOSED);
  default:
    assert(0);
  }
}

