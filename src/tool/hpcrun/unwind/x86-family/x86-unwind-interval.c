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
// Copyright ((c)) 2002-2016, Rice University
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
#include "ui_tree.h"

#include <messages/messages.h>

#include <lib/prof-lean/atomic-op.h>

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
new_ui(char *start, 
       ra_loc ra_status, unsigned int sp_ra_pos, int bp_ra_pos, 
       bp_loc bp_status,          int sp_bp_pos, int bp_bp_pos,
       unwind_interval *prev, mem_alloc m_alloc)
{
  bitree_uwi_t *u = bitree_uwi_new(NULL, prev, NULL, m_alloc);

  // DXN TODO: what is this?
# include "mem_error_gen.h" // **** SPECIAL PURPOSE CODE TO INDUCE MEM FAILURE (conditionally included) ***

  hpcrun_stats_num_unwind_intervals_total_inc();

  interval_t *interval =
	  interval_t_new(start, 0, m_alloc);
  x86recipe_t* x86recipe =
	  x86recipe_new(ra_status, sp_ra_pos, bp_ra_pos, bp_status, sp_bp_pos, bp_bp_pos, m_alloc);
  bitree_uwi_set_rootval(u, uwi_t_new(interval, (uw_recipe_t*)x86recipe, m_alloc));
  return u; 
}


void
set_ui_canonical(unwind_interval *u, unwind_interval *value)
{
  UWI_RECIPE(u)->prev_canonical = value;
} 

void
set_ui_restored_canonical(unwind_interval *u, unwind_interval *value)
{
  UWI_RECIPE(u)->prev_canonical = value;
  UWI_RECIPE(u)->restored_canonical = 1;
} 


unwind_interval *
fluke_ui(char *loc, unsigned int pos, mem_alloc m_alloc)
{
  bitree_uwi_t *u = bitree_uwi_new(NULL, NULL, NULL, m_alloc);
  interval_t *interval =
	  interval_t_new(loc, loc, m_alloc);
  x86recipe_t* x86recipe =
	  x86recipe_new(RA_SP_RELATIVE, pos, 0, 0, 0, 0, m_alloc);
  bitree_uwi_set_rootval(u, uwi_t_new(interval, (uw_recipe_t*)x86recipe, m_alloc));
  return u;

}

void 
link_ui(unwind_interval *current, unwind_interval *next)
{
  UWI_END_ADDR(current) = UWI_START_ADDR(next);
  bitree_uwi_set_rightsubtree(current, next);
}

static void
_dump_ui_str(unwind_interval *u, char *buf, size_t len)
{
  snprintf(buf, len, "UNW: start=%p end =%p ra_status=%s sp_ra_pos=%d sp_bp_pos=%d bp_status=%s "
           "bp_ra_pos = %d bp_bp_pos=%d next=%p prev=%p prev_canonical=%p rest_canon=%d\n"
           "has_tail_calls = %d",
           (void *) UWI_START_ADDR(u), (void *) UWI_END_ADDR(u),
		   ra_status_string(UWI_RECIPE(u)->ra_status),
		   UWI_RECIPE(u)->sp_ra_pos, UWI_RECIPE(u)->sp_bp_pos,
           bp_status_string(UWI_RECIPE(u)->bp_status),
           UWI_RECIPE(u)->bp_ra_pos, UWI_RECIPE(u)->bp_bp_pos,
		   UWI_NEXT(u), UWI_PREV(u),
		   UWI_RECIPE(u)->prev_canonical, UWI_RECIPE(u)->restored_canonical,
           UWI_RECIPE(u)->has_tail_calls);
}


void
dump_ui_log(unwind_interval *u)
{
  char buf[1000];

  _dump_ui_str(u, buf, sizeof(buf));

  EMSG(buf);
}

void
dump_ui(unwind_interval *u, int dump_to_stderr)
{
  if ( ! ENABLED(UNW) ) {
    return;
  }

  char buf[1000];
  _dump_ui_str(u, buf, sizeof(buf));

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

  _dump_ui_str(u, buf, sizeof(buf));

  EEMSG(buf);
}

void
dump_ui_dbg(unwind_interval *u)
{
  char buf[1000];

  _dump_ui_str(u, buf, sizeof(buf));

  fprintf(stderr,"%s\n", buf);
  fflush(stderr);
}

void
dump_ui_troll(unwind_interval *u)
{
  char buf[1000];

  _dump_ui_str(u, buf, sizeof(buf));

  TMSG(TROLL,buf);
}

x86recipe_t*
x86recipe_new(ra_loc ra_status, int sp_ra_pos, int bp_ra_pos,
	 bp_loc bp_status, int sp_bp_pos, int bp_bp_pos, mem_alloc m_alloc)
{
  x86recipe_t* recipe = (x86recipe_t*)m_alloc(sizeof(x86recipe_t));
  recipe->ra_status = ra_status;
  recipe->sp_ra_pos = sp_ra_pos;
  recipe->sp_bp_pos = sp_bp_pos;
  recipe->bp_status = bp_status;
  recipe->bp_bp_pos = bp_bp_pos;
  recipe->bp_ra_pos = bp_ra_pos;
  recipe->prev_canonical = NULL;
  recipe->restored_canonical = 0;
  recipe->has_tail_calls = false;
  return recipe;
}

void
x86recipe_tostr(void* recipe, char str[])
{
  // TODO
  x86recipe_t* x86recipe = (x86recipe_t*)recipe;
  snprintf(str, MAX_RECIPE_STR, "%s%d%s%s",
	  "x86recipe ", x86recipe->sp_ra_pos,
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

