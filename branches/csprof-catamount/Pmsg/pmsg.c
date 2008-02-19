#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "spinlock.h"
#include "pmsg.h"
#include "fname_max.h"

#ifdef CSPROF_THREADS
#include <pthread.h>
extern pthread_mutex_t mylock;
#include "thread_use.h"
#endif

#if 0
#ifdef CSPROF_THREADS
#define WRITE(desc,buf,n) do { \
  if (csprof_using_threads){   \
  pthread_mutex_lock(&mylock); \
  write(desc,buf,n); \
  pthread_mutex_unlock(&mylock); \
} while(0)
#else
#define WRITE(desc,buf,n) do {\
  spinlock_lock(&pmsg_lock); \
  write(desc,buf,n); \
  spinlock_unlock(&pmsg_lock); \
} while (0)
#endif
#endif

#if 0
#ifdef CSPROF_THREADS
#define WRITE(desc,buf,n) do { 	 \
  pthread_mutex_lock(&mylock); 	 \
  fprintf(log_file,buf);     	 \
  fflush(log_file);            	 \
  pthread_mutex_unlock(&mylock); \
} while(0)
#else
#define WRITE(desc,buf,n) do {        \
  spinlock_lock(&pmsg_lock);   \
  fprintf(log_file,buf);              \
  fflush(log_file);                   \
  spinlock_unlock(&pmsg_lock); \
} while (0)
#endif
#endif

#define WRITE(desc,buf,n) do {        \
  spinlock_lock(&pmsg_lock);   \
  fprintf(log_file,buf);              \
  fflush(log_file);                   \
  spinlock_unlock(&pmsg_lock); \
} while (0)

#if 1
#  define INIT_MASK0 TROLL | PAPI
#else
#  define INIT_MASK0 TROLL
#endif
#ifdef DBG_EXTRA
#  define INIT_MASK (DBG | INIT_MASK0)
#else // ! defined DBG_EXTRA
#  define INIT_MASK INIT_MASK0
#endif // DBG_EXTRA

static int __msg_mask = INIT_MASK;
static spinlock_t pmsg_lock = 0;

static FILE *log_file;
FILE *pmsg_db_save_file;

#define LOG_FILE_NAME "csprof.dbg.log"

void pmsg_init(void)
{

  /* Generate a filename */
  char fnm[CSPROF_FNM_SZ];

  sprintf(fnm, "%d.%s",getpid(),LOG_FILE_NAME);

  log_file = fopen(fnm,"w");
  if (! log_file) {
    log_file = stderr;
  }
  
  // tmp[0] = '\0';
  // n = sprintf(tmp,"__msg_mask = 0x%x\n",__msg_mask);
  // spinlock_lock(&pmsg_lock);
  // write(2,tmp,n);
  // spinlock_unlock(&pmsg_lock);
}

void pmsg_fini(void)
{
  if (! (log_file == stderr)) {
    fclose(log_file);
  }
}

void set_mask_f_numstr(const char *s)
{
  int lmask;

  if (sscanf(s,"%d",&lmask)) {
    __msg_mask = lmask;
  }
}

void set_pmsg_mask_f_int(int m)
{
  __msg_mask = m;
}

// mask comes in as string of (space separated) tokens.
// each token is converted to it's code
//
void set_pmsg_mask_f_string(char *s)
{
}

// call from debugger to make pmsg output go to screen
void _dbg_pmsg_stderr(void)
{
  pmsg_db_save_file = log_file;
  log_file          = stderr;
}
void _dbg_pmsg_restore(void)
{
  log_file = pmsg_db_save_file;
}

static void EMSG_internal(const char *format, va_list args) 
{
  char fstr[256];
  char buf[512];
  int n;

#ifdef CSPROF_THREADS
  if (csprof_using_threads) {
    sprintf(fstr,"[%lx]: ", pthread_self());
  }
  else {
    fstr[0] = '\0';
  }
#else
  fstr[0] = '\0';
#endif
  strcat(fstr,format);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  WRITE(2,buf,n);
  va_end(args);
}

void EMSG(const char *format,...)
{
  va_list args;
  va_start(args, format);

  EMSG_internal(format, args);
}

void PMSG(int mask, const char *format, ...)
{
  va_list args;
  va_start(args, format);

  if (! (mask & __msg_mask)) return;

  EMSG_internal(format,args);
}
