/*
 * MSG and friends as actual routines
 */

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

pthread_mutex_t mylock;
extern int csprof_using_threads;

static inline void NOMSG(int level, char *format, ...)
{
}

/* pervasive use of a character buffer because the direct
   printf-to-a-stream functions seem to have problems */

#define WRITE(desc,buf,n) do { \
  if (csprof_using_threads) { \
    pthread_mutex_lock(&mylock); \
    write(desc,buf,n); \
    pthread_mutex_unlock(&mylock); \
  } \
  else { \
    write(desc,buf,n); \
  } \
} while(0)

void MSG(int level,char *fmt, ...){
  va_list args;
  char fstr[256];
  char buf[512];
  va_start(args, fmt);

  int n;

  if(csprof_using_threads){
    sprintf(fstr,"csprof msg [%d][%lx]: ",level, pthread_self());
  }
  else {
    sprintf(fstr,"csprof msg [%d]: ",level);
  }
  strcat(fstr,fmt);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  WRITE(2,buf,n);
  va_end(args);
}

