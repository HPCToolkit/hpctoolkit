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
#include <sys/types.h>
#include <unistd.h>

#include "fname_max.h"
#include "name.h"
#include "pmsg.h"
#include "spinlock.h"
#include "thread_data.h"
#include "thread_use.h"
#include "tokenize.h"

// FIXME: use tbl below to count # flags

#define N_DBG_FLAGS 200
static int dbg_flags[N_DBG_FLAGS];

static char *tbl[] = {
# undef E
# undef D
# define E(s) #s
# define D(s) E(s)
# include "pmsg.src"
# undef E
# undef D
};

#define N_CATEGORIES (sizeof(tbl)/sizeof(tbl[0]))

static int defaults[] = {
  DBG_PREFIX(TROLL)
};
#define NDEFAULTS (sizeof(defaults)/sizeof(defaults[0]))

static void
flag_fill(int v)
{
  for(int i=0; i < N_DBG_FLAGS; i++){
    dbg_flags[i] = v;
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
turn_on_all(void)
{
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

static FILE *log_file;
// FILE *pmsg_db_save_file;

#define LOG_FILE_EXT "dbg.log"

static spinlock_t pmsg_lock = SPINLOCK_UNLOCKED;

static void
dump(void){
  for (int i=0; i < N_DBG_FLAGS; i++){
    if (i < N_CATEGORIES){
      fprintf(stderr,"dbg_flags[%s] = %d\n",tbl[i],dbg_flags[i]);
    } else {
      fprintf(stderr,"dbg_flags[UNK] = %d\n",i);
    }
  }
}

void
pmsg_init(char *exec_name)
{
  /* Generate a filename */
  char fnm[CSPROF_FNM_SZ];

  sprintf(fnm, "%s.%lx-%u.%s", exec_name, gethostid(), getpid(), LOG_FILE_EXT);

  log_file = fopen(fnm,"w");
  if (! log_file) {
    log_file = stderr;
  }
  flag_fill(0);
  for (int i=0; i < NDEFAULTS; i++){
    dbg_flags[defaults[i]] = 1;
  }

  char *s = getenv("CSPROF_DD");
  if(s){
    csprof_dbg_init(s);
  }
  spinlock_unlock(&pmsg_lock);
  // dump();
}

void
pmsg_fini(void)
{
  if (! (log_file == stderr)) {
    fclose(log_file);
  }
}

static void
_msg(const char *fmt,va_list args)
{
  char fstr[256];
  char buf[512];
  int n;

  fstr[0] = '\0';
  if (csprof_using_threads){
    sprintf(fstr,"[%d]: ",TD_GET(id));
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
csprof_dbg(pmsg_category flag)
{
  return dbg_flags[flag];
}


int
csprof_logfile_fd(void)
{
  return fileno(log_file);
}
