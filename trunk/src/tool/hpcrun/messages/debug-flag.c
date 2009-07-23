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
#include "tokenize.h"



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
 E(UNW),
 E(UITREE),
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
 E(DBG),
 E(THREAD),
 E(PROCESS),
 E(EPOCH),
 E(HANDLING_SAMPLE),
 E(MEM),
 E(MEM2),
 E(SAMPLE_FILTER),
 E(THREAD_SPECIFIC),
 E(DL_BOUND),
 E(ADD_MODULE_BASE),
 E(DL_ADD_MODULE),
 E(OPTIONS),
 E(PRE_FORK),
 E(POST_FORK),
 E(EVENTS),
 E(SYSTEM_SERVER),
 E(SYSTEM_COMMAND),
 E(AS_add_source),
 E(AS_started),
 E(AS_MAP),
 E(SS_COMMON),
 E(METRICS),
 E(UNW_CONFIG),
 E(UNW_STRATEGY),
 E(BACKTRACE),
 E(SUSPENDED_SAMPLE),
 E(MMAP),
 E(MALLOC),
 E(CSP_MALLOC),
 E(MEM__ALLOC)
};


static flag_list_t all_list = {
  .n_entries = sizeof(all_list_entries)/sizeof(all_list_entries[0]),
  .entries   = all_list_entries
};


#define N_DBG_CATEGORIES sizeof(dbg_tbl)/sizeof(dbg_tbl[0])
static int dbg_flags[N_DBG_CATEGORIES];


static int defaults[] = {
  DBG_PREFIX(TROLL),
  DBG_PREFIX(DROP),
  DBG_PREFIX(SUSPICIOUS_INTERVAL)
};
#define NDEFAULTS (sizeof(defaults)/sizeof(defaults[0]))



//*****************************************************************************
// forward declarations 
//*****************************************************************************

static void debug_flag_set_all(int v);

static void debug_flag_process_string(char *in, int debug_initialization);

static void debug_flag_process_env(int debug_initialization);



//*****************************************************************************
// interface operations 
//*****************************************************************************

void debug_flag_init()
{
  char *df_trace = getenv("HPCRUN_DEBUG_FLAGS_DEBUG");

  debug_flag_set_all(0);
  debug_flag_process_env(df_trace);

  if (df_trace){
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
debug_flag_set_list(flag_list_t *flag_list,int v)
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
    if (strcmp(f,"ALL") == 0){
      debug_flag_set_list(&all_list, 1);
      return;
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
    } else {
      fprintf(stderr, "\tdebug flag token %s not recognized\n\n", f);
    }
  }
}


static void 
debug_flag_process_env(int debug_initialization)
{
  if (getenv("HPCRUN_QUIET") != NULL){
    for (int i=0; i < NDEFAULTS; i++){
      debug_flag_set(defaults[i], 1);
    }
  }

  char *s = getenv("HPCRUN_DEBUG_FLAGS");
  if(s){
    debug_flag_process_string(s, debug_initialization);
  }
}


static void
debug_flag_dump(void)
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
