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

#include "fname_max.h"
#include "files.h"
#include "name.h"
#include "pmsg.h"
#include "spinlock.h"
#include "thread_data.h"
#include "thread_use.h"
#include "tokenize.h"

// FIXME: use tbl below to count # flags

static FILE *log_file;
static spinlock_t pmsg_lock = SPINLOCK_UNLOCKED;

static char *tbl[] = {
# undef E
# define E(s) #s
# include "pmsg.src"
# undef E
};

#define N_PMSG_FLAGS sizeof(tbl)/sizeof(tbl[0])
static int dbg_flags[N_PMSG_FLAGS];

static char *ctl_tbl[] = {
# undef D
# define D(s) #s
# include "ctl.src"
# undef D
};

#define N_CTL_CATEGORIES sizeof(ctl_tbl)/sizeof(ctl_tbl[0])
static int ctl_flags[N_CTL_CATEGORIES];

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

#define N_CATEGORIES (sizeof(tbl)/sizeof(tbl[0]))

static int defaults[] = {
  DBG_PREFIX(TROLL),
  DBG_PREFIX(UNW_CONFIG)
};
#define NDEFAULTS (sizeof(defaults)/sizeof(defaults[0]))

static void
flag_fill(int v)
{
  for(int i=0; i < N_PMSG_FLAGS; i++){
    dbg_flags[i] = v;
  }
}

static void
ctl_flag_fill(int v)
{
  for(int i=0; i < N_CTL_CATEGORIES; i++){
    ctl_flags[i] = v;
  }
}

static int
lookup(char *s)
{
  for (int i=0; i < N_CATEGORIES; i++){
    if (! strcmp(tbl[i],s)){
      return i;
    }
  }
  return -1;
}

static void
csprof_dbg_init(char *in)
{
  // csprof_emsg("input string f init = %s",in);
  for (char *f=start_tok(in); more_tok(); f=next_tok()){
    if (strcmp(f,"ALL") == 0){
      flag_fill(1);
      return;
    }
    // csprof_emsg("checking token %s",f);
    int ii = lookup(f);
    if (ii >= 0){
      // csprof_emsg("token code = %d",ii);
      dbg_flags[ii] = 1;
    } else {
      fprintf(stderr,"WARNING: dbg flag %s not recognized\n",f);
    }
  }
}

static void
dump(void)
{
  for (int i=0; i < N_PMSG_FLAGS; i++){
    if (i < N_CATEGORIES){
      fprintf(stderr,"dbg_flags[%s] = %d\n",tbl[i],dbg_flags[i]);
    } else {
      fprintf(stderr,"dbg_flags[UNK] = %d\n",i);
    }
  }
}


void
pmsg_init()
{
  // get name for log file 
  char log_name[PATH_MAX];
  files_log_name(log_name, PATH_MAX);

  // open log file
  log_file = fopen(log_name,"w");
  if (!log_file) log_file = stderr;

  flag_fill(0);
  for (int i=0; i < NDEFAULTS; i++){
    dbg_flags[defaults[i]] = 1;
  }
  ctl_flag_fill(0);

  char *s = getenv("CSPROF_DD");
  if(s){
    csprof_dbg_init(s);
  }
  spinlock_unlock(&pmsg_lock);
}

void
pmsg_fini(void)
{
  if (log_file != stderr) fclose(log_file);
}

static void
_msg(const char *fmt,va_list args)
{
  char fstr[256];
  char buf[512];
  int n;

  fstr[0] = '\0';
  if (csprof_using_threads_p()){
    sprintf(fstr,"[%d]: ",TD_GET(id));
  }
  IF_ENABLED(PID){
    sprintf(fstr,"[%d]: ",getpid());
  }
  strcat(fstr,fmt);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  spinlock_lock(&pmsg_lock);
  fprintf(log_file,buf);
  fflush(log_file);
  spinlock_unlock(&pmsg_lock);

  va_end(args);
}


// like _msg, but doesn't care about threads ...
static void
_nmsg(const char *fmt,va_list args)
{
  char fstr[256];
  char buf[512];
  int n;

  fstr[0] = '\0';
  strcat(fstr,fmt);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  spinlock_lock(&pmsg_lock);
  fprintf(log_file,buf);
  fflush(log_file);
  spinlock_unlock(&pmsg_lock);

  va_end(args);
}

void
csprof_emsg_valist(const char *fmt, va_list args)
{
  _msg(fmt,args);
}


void
csprof_emsg(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  _msg(fmt,args);
}

void
csprof_exit_on_error(int ret, int ret_expected, const char *fmt, ...)
{
  if (ret == ret_expected) {
    return;
  }
  va_list args;
  va_start(args,fmt);
  _msg(fmt,args);
  abort();
}

void
csprof_abort_w_info(void (*info)(void),const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  _msg(fmt,args);

  va_start(args,fmt);
  vfprintf(stderr,fmt,args);
  va_end(args);
  fprintf(stderr,"\n");
  info();
  exit(-1);
}

// message to log file, also echo on stderr
void
csprof_stderr_msg(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  _msg(fmt,args);

  va_start(args,fmt);
  vfprintf(stderr,fmt,args);
  va_end(args);
  fprintf(stderr,"\n");
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
  _msg(fmt,args);
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

// interface to the debug flags
int
csprof_dbg(ctl_category flag)
{
  return ctl_flags[flag];
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
