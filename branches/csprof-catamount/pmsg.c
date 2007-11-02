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
#endif

#ifdef CSPROF_THREADS
#define WRITE(desc,buf,n) do { \
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

static int __msg_mask = NONE;
static simple_spinlock pmsg_lock = 0;

#define N_DEBUG_FLAGS 200
static int _flag_array[N_DEBUG_FLAGS];

static void _fill(int v){
  int i;
  for (i = 0; i < N_DEBUG_FLAGS; i++){
    _flag_array[i] = v;
  }
}

void pmsg_init(void){
  char tmp[100];
  int n;

#if 1
  __msg_mask = TROLL | BASIC_INIT;
#endif

  // tmp[0] = '\0';
  // n = sprintf(tmp,"__msg_mask = 0x%x\n",__msg_mask);
  // simple_spinlock_lock(&pmsg_lock);
  // write(2,tmp,n);
  // simple_spinlock_unlock(&pmsg_lock);
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
  sprintf(fstr,"[%lx]: ", pthread_self());
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
  char fstr[256];
  char buf[512];
  va_start(args, format);
  int n;

  if (! (mask & __msg_mask)){
    return;
  }

#ifdef CSPROF_THREADS
  sprintf(fstr,"[%lx]: ", pthread_self());
#else
  fstr[0] = '\0';
#endif
  strcat(fstr,format);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  WRITE(2,buf,n);
  va_end(args);
}
// #endif
