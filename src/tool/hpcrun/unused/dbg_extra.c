#include <dlfcn.h>
#include <pthread.h>

#include "pmsg.h"

typedef void *(*getspecif_t)(pthread_key_t k);
typedef int  (*setspecif_t)(pthread_key_t k, const void *p);

static getspecif_t __real_pthread_getspecific;
static setspecif_t __real_pthread_setspecific;

void
dbg_init(void)
{
  __real_pthread_getspecific = dlsym(RTLD_NEXT,"pthread_getspecific");
  __real_pthread_setspecific = dlsym(RTLD_NEXT,"pthread_setspecific");
}

void *
pthread_getspecific(pthread_key_t k)
{
  pthread_t tid = pthread_self();
  void *    ret = __real_pthread_getspecific(k);
  PMSG(DBG,"%0x\tGET\tkey(%0x)=%p\tTSD",tid,k,ret);
  return ret;
}

int
pthread_setspecific(pthread_key_t k, const void *p)
{
  pthread_t tid = pthread_self();
  void *    ret = __real_pthread_setspecific(k,p);
  PMSG(DBG,"%0x\tSET\tkey(%0x)=%p\tTSD",tid,k,p);
  return ret;
}
