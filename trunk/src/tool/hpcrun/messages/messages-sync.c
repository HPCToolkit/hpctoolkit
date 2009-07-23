//*****************************************************************************
// File: messages-sync.c 
//
// Description:
//   This half of the messaging system. It contains parts that need not be
//   async-signal safe. 
//
// History:
//   19 July 2009 
//     created by partitioning pmsg.c into pmsg-sync.c and pmsg-async.c
//
//*****************************************************************************


//*****************************************************************************
// global includes 
//*****************************************************************************

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>



//*****************************************************************************
// local includes 
//*****************************************************************************

#include "disabled.h"
#include "fname_max.h"
#include "files.h"
#include "name.h"
#include "messages.h"
#include "messages.i"
#include "thread_data.h"
#include "thread_use.h"
#include "tokenize.h"
#include "monitor.h"


//*****************************************************************************
// global variables 
//*****************************************************************************

FILE *log_file;

static char *dbg_tbl[] = {
# undef E
# define E(s) #s
# include "messages.flag-defns"
# undef E
};

static char *__csprof_noisy_msgs = NULL;

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



//-------------------------------------
// Log output may be throttled by using 
// the message limiting mechanism
//-------------------------------------
static int global_msg_count = 0;

//-------------------------------------
// how many unwind msg blocks to permit 
// (500 is reasonable choice)
// FIXME: make this an option
//-------------------------------------
static int const threshold = 500;

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

static void debug_flag_process_string(char *in);

static void debug_flag_process_env();

void debug_flag_init();



//*****************************************************************************
// interface operations 
//*****************************************************************************

void
messages_init()
{
  __csprof_noisy_msgs = getenv("__CSPROF_NOISY_MSGS");

  debug_flag_init();
  spinlock_unlock(&pmsg_lock); // initialize lock for async operations

  if (__csprof_noisy_msgs){
    monitor_real_exit(1);
  }

  log_file = stderr;
}


void
messages_create_logfile()
{
  if (hpcrun_get_disabled()) return;

  // get name for log file 
  char log_name[PATH_MAX];
  files_log_name(log_name, 0, PATH_MAX);

  // open log file
  log_file = fopen(log_name,"w");
  if (!log_file) log_file = stderr; // reset to stderr
}


void
messages_fini(void)
{
  if (hpcrun_get_disabled()) return;

  if (log_file != stderr){
    fclose(log_file);

    //----------------------------------------------------------------------
    // if this is an execution of an MPI program, we opened the log file 
    // before the MPI rank was known. thus, the name of the log file is 
    // missing the MPI rank. fix that now by renaming the log file to what 
    // it should be.
    //----------------------------------------------------------------------
    int rank = monitor_mpi_comm_rank();
    if (rank >= 0) {
      char old[PATH_MAX];
      char new[PATH_MAX];
      files_log_name(old, 0, PATH_MAX);
      files_log_name(new, rank, PATH_MAX);
      rename(old, new);
    }
  }
}


void debug_flag_init()
{
  debug_flag_set_all(0);
  debug_flag_process_env();
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
csprof_exit_on_error(int ret, int ret_expected, const char *fmt, ...)
{
  if (ret == ret_expected) {
    return;
  }
  va_list args;
  va_start(args,fmt);
  hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
  abort();
}


void
csprof_abort_w_info(void (*info)(void), const char *fmt, ...)
{
  // massage fmt string to end in a newline
  char fstr[MSG_BUF_SIZE];

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - strlen(fstr) - 5);
  strcat(fstr,"\n");

  va_list args;

  if (log_file != stderr) {
    va_start(args, fmt);
    hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
  }

  va_start(args,fmt);
  vfprintf(stderr, fstr, args);
  va_end(args);
  info();
  monitor_real_exit(-1);
}


// message to log file, also echo on stderr
void
csprof_stderr_log_msg(bool copy_to_log, const char *fmt, ...)
{
  // massage fmt string to end in a newline
  char fstr[MSG_BUF_SIZE];
  va_list args;

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - 5);
  strcat(fstr,"\n");

  va_start(args, fmt);
  vfprintf(stderr, fstr, args);
  va_end(args);

  if (copy_to_log && log_file != stderr){
    va_list args;
    va_start(args, fmt);
    hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
  }
}


void
__csprof_dc(void)
{
  ;
}


int
csprof_logfile_fd(void)
{
  return fileno(log_file);
}


int
csprof_below_pmsg_threshold(void)
{
  return (global_msg_count < threshold);
}


void
csprof_up_pmsg_count(void)
{
  global_msg_count++;
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
debug_flag_process_string(char *in)
{
  if (__csprof_noisy_msgs) hpcrun_emsg("dd input string f init = %s",in);
  for (char *f=start_tok(in); more_tok(); f=next_tok()){
    if (strcmp(f,"ALL") == 0){
      debug_flag_set_list(&all_list, 1);
      return;
    }
    if (__csprof_noisy_msgs) hpcrun_emsg("checking dd token %s",f);
    int ii = debug_flag_name_lookup(f);
    if (ii >= 0){
      if (__csprof_noisy_msgs) hpcrun_emsg("dd token code = %d",ii);
      debug_flag_set(ii,1);
    } else {
      fprintf(stderr,"WARNING: dbg flag %s not recognized\n",f);
    }
  }
}

static void 
debug_flag_process_env()
{
  if (! getenv("CSPROF_QUIET")){
    for (int i=0; i < NDEFAULTS; i++){
      debug_flag_set(defaults[i], 1);
    }
  }

  char *s = getenv("CSPROF_DD");
  if(s){
    debug_flag_process_string(s);
  }
}


#ifdef DBG_PMSG
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
#endif

