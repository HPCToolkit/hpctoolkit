#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "simple-lock.h"
#include "pmsg2.h"
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

static simple_spinlock pmsg_lock = 0;

#define N_DEBUG_FLAGS 200
static int _flag_array[N_DEBUG_FLAGS];

typedef struct _item_s {
  char *key;
  unsigned int val;
} _item_t;

static _item_t lookup[] = {
  "foo",0,
  "bar",1
};

static int _find(char *s){
  int i;
  _item_t *item;

  for(i=0,item = &lookup[0]; i < sizeof(lookup)/sizeof(_item_t); item++,i++){
    // printf("item[%d] = %s -> %d\n",i,item->key,item->val);
    if (! strcmp(s,item->key)){
      return item->val;
    }
  }
  return -1;
}

static void _fill(int v){
  int i;
  for (i = 0; i < N_DEBUG_FLAGS; i++){
    _flag_array[i] = v;
  }
}

void pmsg_init(char *init){
  char tmp[100];
  int n;

  _fill(0);

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

void PMSG(int category, const char *format, ...){
  // #if defined(CSPROF_PERF)
  //}
  //#else

  va_list args;
  char fstr[256];
  char buf[512];
  va_start(args, format);
  int n;

  if (! _flag_array[category]){
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
