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
// Copyright ((c)) 2002-2022, Rice University
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

//*****************************************************************************
// File: debug-flag.c
//
// Description:
//   debug flags management for hpcrun
//
// History:
//   23 July 2009 - John Mellor-Crummey 
//     created by splitting off from message-sync.c
//
//*****************************************************************************



//*****************************************************************************
// global includes 
//*****************************************************************************

#include <stdlib.h>
#include <string.h>
#include <stdio.h>



//*****************************************************************************
// local includes 
//*****************************************************************************

#include "messages/debug-flag.h"
#include "monitor.h"
#include <utilities/tokenize.h>

extern void unlimit_msgs(void);

//*****************************************************************************
// global variables 
//*****************************************************************************

static char *dbg_tbl[] = {
# undef E
# define E(s) #s
# include "messages.flag-defns"
# undef E
};


typedef struct flag_list_t {
  int n_entries;
  pmsg_category *entries;
} flag_list_t;


static pmsg_category all_list_entries [] = {
#define E(s) DBG_PREFIX(s)
 E(INIT),
 E(FINI),
 E(FIND),
 E(INTV),
 E(ITIMER_HANDLER),
 E(ITIMER_CTL),
 E(CPU_GPU_BLAME_CTL),
 E(_TST_HANDLER),
 E(_TST_CTL),
 E(UNW),
 // E(UW_RECIPE_MAP),
 // E(TRAMP), // Writing messages to logs in trampoline can lead to execution errors.
 E(RETCNT_CTL),
 E(SEGV),
 E(MPI),
 E(INTV2),
 E(INTV_ERR),
 E(TROLL),
 E(DROP),
 E(UNW_INIT),
 E(BASIC_INIT),
 E(SAMPLE),
 E(SWIZZLE),
 E(FINALIZE),
 E(DATA_WRITE),
 E(PAPI),
 E(PAPI_SAMPLE),
 E(PAPI_EVENT_NAME),
 E(UPC),
 E(DBG),
 E(THREAD),
 E(PROCESS),
 E(LOADMAP),
 E(HANDLING_SAMPLE),
 // E(MEM),
 // E(MEM2),
 E(SAMPLE_FILTER),
 // E(THREAD_SPECIFIC),
 E(THREAD_CTXT),
 E(DL_BOUND),
 E(ADD_MODULE_BASE),
 E(DL_ADD_MODULE),
 E(OPTIONS),
 // E(PRE_FORK),
 // E(POST_FORK),
 E(EVENTS),
 // E(SYSTEM_SERVER),
 // E(AS_add_source),
 // E(AS_started),
 // E(AS_MAP),
 // E(SS_COMMON),
 E(METRICS),
 E(UNW_CONFIG),
 E(UNW_STRATEGY),
 E(BACKTRACE),
 E(SUSPENDED_SAMPLE),
 // E(MMAP),
 // E(MALLOC),
 // E(CSP_MALLOC),
 // E(MEM__ALLOC),
 E(NORM_IP),
 E(PARTIAL_UNW) 
};


static flag_list_t all_list = {
  .n_entries = sizeof(all_list_entries)/sizeof(all_list_entries[0]),
  .entries   = all_list_entries
};


#define N_DBG_CATEGORIES sizeof(dbg_tbl)/sizeof(dbg_tbl[0])
static int dbg_flags[N_DBG_CATEGORIES];



//*****************************************************************************
// forward declarations 
//*****************************************************************************

static void debug_flag_set_all(int v);

static void debug_flag_process_string(char *in, int debug_initialization);

static void debug_flag_process_env(int debug_initialization);

static const char *debug_flag_name_get(int i);


//*****************************************************************************
// interface operations 
//*****************************************************************************

void debug_flag_init()
{
  char *df_trace = getenv("HPCRUN_DEBUG_FLAGS_DEBUG");
  int debug_mode_only = (df_trace == 0) ? 0 : 1;

  debug_flag_set_all(0);
  debug_flag_process_env(debug_mode_only);

  if (debug_mode_only){
    monitor_real_exit(1);
  }
}


void
debug_flag_set(pmsg_category flag, int val)
{
  dbg_flags[flag] = val;
}


int
debug_flag_get(pmsg_category flag)
{
  return dbg_flags[flag];
}


void
debug_flag_dump()
{
  for (int i=0; i < N_DBG_CATEGORIES; i++){
    if (i < N_DBG_CATEGORIES){
      fprintf(stderr,"debug flag %s = %d\n", debug_flag_name_get(i), 
	      debug_flag_get(i));
    } else {
      fprintf(stderr,"debug flag (unknown) = %d\n", i);
    }
  }
}



//*****************************************************************************
// private operations 
//*****************************************************************************

static void
debug_flag_set_all(int v)
{
  for(int i=0; i < N_DBG_CATEGORIES; i++){
    debug_flag_set(i, v);
  }
}


static void
debug_flag_set_list(flag_list_t *flag_list, int v)
{
  for (int i=0; i < flag_list->n_entries; i++){
    debug_flag_set(flag_list->entries[i], v);
  }
}


static const char *
debug_flag_name_get(int i)
{
  const char *result = NULL;
  if (i < N_DBG_CATEGORIES) {
    result = dbg_tbl[i];
  }
  return result; 
}


static int
debug_flag_name_lookup(const char *s)
{
  for (int i = 0; i < N_DBG_CATEGORIES; i++){
    if (strcmp(dbg_tbl[i],s) == 0){
      return i;
    }
  }
  return -1;
}


static void
debug_flag_process_string(char *in, int debug_initialization)
{
  if (debug_initialization) {
    fprintf(stderr, "debug flag input string = %s\n\n", in);
  }

  for (char *f=start_tok(in); more_tok(); f = next_tok()){
    unlimit_msgs();
    if (strcmp(f,"ALL") == 0){
      debug_flag_set_list(&all_list, 1);
      continue;
    }
    if (debug_initialization) {
      fprintf(stderr, "\tprocessing debug flag token %s\n", f);
    }
    int ii = debug_flag_name_lookup(f);
    if (ii >= 0){
      if (debug_initialization) {
	fprintf(stderr, "\tdebug flag token value = %d\n\n", ii);
      }
      debug_flag_set(ii,1);
    }
    else {
      fprintf(stderr, "\tdebug flag token %s not recognized\n\n", f);
    }
  }
}


static void 
debug_flag_process_env(int debug_initialization)
{
  char *s = getenv("HPCRUN_DEBUG_FLAGS");
  if(s){
    debug_flag_process_string(s, debug_initialization);
  }
}

