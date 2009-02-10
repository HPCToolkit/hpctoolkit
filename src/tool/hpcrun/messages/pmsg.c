//
// Use an array of flags to turn on/off printing
//
// Also supply EMSG for un restricted printing
//
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#include "fname_max.h"
#include "files.h"
#include "name.h"
#include "pmsg.h"
#include "spinlock.h"
#include "thread_data.h"
#include "thread_use.h"
#include "tokenize.h"
#include "monitor.h"

static FILE *log_file;
static spinlock_t pmsg_lock = SPINLOCK_UNLOCKED;

static char *dbg_tbl[] = {
# undef E
# define E(s) #s
# include "pmsg.src"
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

#if 0
static char *ctl_tbl[] = {
# undef D
# define D(s) #s
# include "ctl.src"
# undef D
};

#define N_CTL_CATEGORIES sizeof(ctl_tbl)/sizeof(ctl_tbl[0])
static int ctl_flags[N_CTL_CATEGORIES];
#endif

void
dbg_set_flag(pmsg_category flag,int val)
{
  dbg_flags[flag] = val;
}

int
dbg_get_flag(pmsg_category flag)
{
  return dbg_flags[flag];
}

static int defaults[] = {
  DBG_PREFIX(TROLL)
};
#define NDEFAULTS (sizeof(defaults)/sizeof(defaults[0]))

static void
flag_fill(int v)
{
  for(int i=0; i < N_DBG_CATEGORIES; i++){
    dbg_flags[i] = v;
  }
}

static void
selected_flag_fill(flag_list_t *flag_list,int v)
{
  for (int i=0; i < flag_list->n_entries; i++){
    dbg_flags[flag_list->entries[i]] = v;
  }
}

static int
lookup(char *s,char *tbl[],int n_entries)
{
  for (int i=0; i < n_entries; i++){
    if (! strcmp(tbl[i],s)){
      return i;
    }
  }
  return -1;
}

static void
csprof_dbg_init(char *in)
{
  if (__csprof_noisy_msgs) csprof_emsg("dd input string f init = %s",in);
  for (char *f=start_tok(in); more_tok(); f=next_tok()){
    if (strcmp(f,"ALL") == 0){
      selected_flag_fill(&all_list,1);
      return;
    }
    if (__csprof_noisy_msgs) csprof_emsg("checking dd token %s",f);
    int ii = lookup(f,dbg_tbl,N_DBG_CATEGORIES);
    if (ii >= 0){
      if (__csprof_noisy_msgs) csprof_emsg("dd token code = %d",ii);
      dbg_flags[ii] = 1;
    } else {
      fprintf(stderr,"WARNING: dbg flag %s not recognized\n",f);
    }
  }
}

#if 0
static void
ctl_flag_fill(int v)
{
  for(int i=0; i < N_CTL_CATEGORIES; i++){
    ctl_flags[i] = v;
  }
}

static void
csprof_ctl_init(char *in)
{
  if (__csprof_noisy_msgs) csprof_emsg("dc input string f init = %s",in);
  for (char *f=start_tok(in); more_tok(); f=next_tok()){
    if (__csprof_noisy_msgs) csprof_emsg("checking dc token %s",f);
    int ii = lookup(f,ctl_tbl,N_CTL_CATEGORIES);
    if (ii >= 0){
      if (__csprof_noisy_msgs) csprof_emsg("dc token code = %d",ii);
      ctl_flags[ii] = 1;
    } else {
      fprintf(stderr,"WARNING: ctl flag %s not recognized\n",f);
    }
  }
}

#endif

// interface to the debug ctl flags
int
csprof_dbg(dbg_category flag)
{
  return dbg_flags[flag];
}

static void
dump(void)
{
  for (int i=0; i < N_DBG_CATEGORIES; i++){
    if (i < N_DBG_CATEGORIES){
      fprintf(stderr,"dbg_flags[%s] = %d\n",dbg_tbl[i],dbg_flags[i]);
    } else {
      fprintf(stderr,"dbg_flags[UNK] = %d\n",i);
    }
  }
}


void
pmsg_init()
{
  __csprof_noisy_msgs = getenv("__CSPROF_NOISY_MSGS");

  // get name for log file 
  char log_name[PATH_MAX];
  files_log_name(log_name, PATH_MAX);

  // open log file
  log_file = fopen(log_name,"w");
  if (!log_file) log_file = stderr;

  flag_fill(0);
  if (! getenv("CSPROF_QUIET")){
    for (int i=0; i < NDEFAULTS; i++){
      dbg_flags[defaults[i]] = 1;
    }
  }

  char *s = getenv("CSPROF_DD");
  if(s){
    csprof_dbg_init(s);
  }
#if 0
  ctl_flag_fill(0);
  s = getenv("CSPROF_DC");
  if (s){
    csprof_ctl_init(s);
  }
#endif
  spinlock_unlock(&pmsg_lock);

  if (__csprof_noisy_msgs){
    monitor_real_exit(1);
  }
}



void
pmsg_fini(void)
{
  if (log_file != stderr) fclose(log_file);
}

#define MSG_BUF_SIZE  4096

static void
write_msg_to_log(const char *fmt,va_list args)
{
  char fstr[MSG_BUF_SIZE];
  char buf[MSG_BUF_SIZE];

  fstr[0] = '\0';
  if (csprof_using_threads_p()){
    sprintf(fstr,"[%d]: ",TD_GET(id));
  }
  if (ENABLED(PID)){
    sprintf(fstr,"[%d]: ",getpid());
  }
  strncat(fstr, fmt, MSG_BUF_SIZE - strlen(fstr) - 5);
  strcat(fstr,"\n");
  vsnprintf(buf, MSG_BUF_SIZE - 2, fstr, args);
  spinlock_lock(&pmsg_lock);
  fprintf(log_file, "%s", buf);
  fflush(log_file);
  spinlock_unlock(&pmsg_lock);

  va_end(args);
}


// like _msg, but doesn't care about threads ...
static void
_nmsg(const char *fmt,va_list args)
{
  char fstr[MSG_BUF_SIZE];
  char buf[MSG_BUF_SIZE];

  fstr[0] = '\0';
  strncat(fstr, fmt, MSG_BUF_SIZE - 5);
  strcat(fstr,"\n");
  vsnprintf(buf, MSG_BUF_SIZE - 2, fstr, args);
  spinlock_lock(&pmsg_lock);
  fprintf(log_file, "%s", buf);
  fflush(log_file);
  spinlock_unlock(&pmsg_lock);

  va_end(args);
}

void
csprof_emsg_valist(const char *fmt, va_list args)
{
  write_msg_to_log(fmt,args);
}


void
csprof_emsg(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  write_msg_to_log(fmt,args);
}

void
csprof_exit_on_error(int ret, int ret_expected, const char *fmt, ...)
{
  if (ret == ret_expected) {
    return;
  }
  va_list args;
  va_start(args,fmt);
  write_msg_to_log(fmt,args);
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
  va_start(args, fmt);
  write_msg_to_log(fmt,args);

  va_start(args,fmt);
  vfprintf(stderr, fstr, args);
  va_end(args);
  info();
  monitor_real_exit(-1);
}

// message to log file, also echo on stderr
void
csprof_stderr_log_msg(bool copy_to_log,const char *fmt, ...)
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

  if (copy_to_log){
    va_list args;
    va_start(args, fmt);
    write_msg_to_log(fmt, args);
  }
}

void
__csprof_dc(void)
{
  ;
}

void
csprof_pmsg(pmsg_category flag,const char *fmt,...)
{
  if (! dbg_flags[flag]){
    // csprof_emsg("PMSG flag in = %d (%s), flag ctl = %d --> NOPRINT",flag,tbl[flag],dbg_flags[flag]);
    return;
  }
  va_list args;
  va_start(args,fmt);
  write_msg_to_log(fmt,args);
}

void
csprof_nmsg(pmsg_category flag,const char *fmt,...)
{
  if (! dbg_flags[flag]){
    return;
  }
  va_list args;
  va_start(args,fmt);
  _nmsg(fmt,args);
}

void
csprof_amsg(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  _nmsg(fmt,args);
}

int
csprof_logfile_fd(void)
{
  return fileno(log_file);
}

//
// Log output may be throttled by using the message limiting mechanism
//

static int global_msg_count = 0;

//
// how many unwind msg blocks to permit (500 is reasonable choice
// FIXME: make this an option
//
static int const threshold = 500;

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
