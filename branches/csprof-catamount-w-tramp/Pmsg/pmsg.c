#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "simple-lock.h"
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
  simple_spinlock_lock(&pmsg_lock); \
  write(desc,buf,n); \
  simple_spinlock_unlock(&pmsg_lock); \
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
  simple_spinlock_lock(&pmsg_lock);   \
  fprintf(log_file,buf);              \
  fflush(log_file);                   \ 
  simple_spinlock_unlock(&pmsg_lock); \
} while (0)
#endif
#endif

#define WRITE(desc,buf,n) do {        \
  simple_spinlock_lock(&pmsg_lock);   \
  fprintf(log_file,buf);              \
  fflush(log_file);                   \ 
  simple_spinlock_unlock(&pmsg_lock); \
} while (0)

static int __msg_mask = NONE;
static simple_spinlock pmsg_lock = 0;

static FILE *log_file;

#define LOG_FILE_NAME "csprof.dbg.log"

void pmsg_init(void){

  __msg_mask = TROLL;

  log_file = fopen(LOG_FILE_NAME,"w");
  if (! log_file){
    log_file = stderr;
  }
  
  // tmp[0] = '\0';
  // n = sprintf(tmp,"__msg_mask = 0x%x\n",__msg_mask);
  // simple_spinlock_lock(&pmsg_lock);
  // write(2,tmp,n);
  // simple_spinlock_unlock(&pmsg_lock);
}

void pmsg_fini(void){
  if (! (log_file == stderr)){
    fclose(log_file);
  }
}

void set_mask_f_numstr(const char *s){
  int lmask;

  if (sscanf(s,"%d",&lmask)){
    __msg_mask = lmask;
  }
}

void set_pmsg_mask_f_int(int m){
  __msg_mask = m;
}

// mask comes in as string of (space separated) tokens.
// each token is converted to it's code
//
void set_pmsg_mask_f_string(char *s){
}

void EMSG(const char *format,...){
  va_list args;
  char fstr[256];
  char buf[512];
  va_start(args, format);
  int n;

#ifdef CSPROF_THREADS
  if (csprof_using_threads){
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

void PMSG(int mask, const char *format, ...){
  // #if defined(CSPROF_PERF)
  //}
  //#else

  va_list args;

  if (! (mask & __msg_mask)){
    return;
  }
  EMSG(format,args);
}
// #endif
