#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "pmsg.h"

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
#define WRITE(desc,buf,n) write(desc,buf,n)
#endif

static int __msg_mask = NONE;

void set_mask_f_numstr(const char *s){
  int lmask;

  if (sscanf(s,"%d",&lmask)){
    __msg_mask = lmask;
  }
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

void PMSG(int mask, const char *format, ...)
#if defined(CSPROF_PERF)
{
}
#else
{
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
#endif
