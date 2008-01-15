#include <pthread.h>

#include "general.h"
#include "killsafe.h"
#include "monitor.h"
#include "name.h"
#include "epoch.h"
#include "dl_bound.h"

#ifdef LINUX
#include <linux/unistd.h>
#endif

#define M(s) write(2,s"\n",strlen(s)+1)

void csprof_init_internal(void);
void csprof_fini_internal(void);
void csprof_pthread_init_data(void);


void monitor_init_process(char *process,int *argc,char **argv,unsigned pid){
  csprof_set_executable_name(process);
  csprof_init_internal();
}

void monitor_fini_process(void){
  // M("monitor calling csprof_fini_internal");
  csprof_fini_internal();
}

void monitor_init_library(void){
  //  extern void csprof_init_internal(void);
  // M("monitor init lib (NOT) calling csprof_init_internal");
}

void monitor_fini_library(void){
  //  extern void csprof_fini_internal(void);
}

// always need this variable, but only init thread support will turn it on
int csprof_using_threads = 0;

#ifdef CSPROF_THREADS
#include "thread_data.h"
pthread_key_t k;

static int thr_c = 0;
static pthread_once_t iflg = PTHREAD_ONCE_INIT;

pthread_mutex_t mylock;

static void n_init(void){
  int e;
  // e = pthread_key_create(&k,free);
  e = pthread_key_create(&k,NULL);
}

void csprof_init_thread_support(int id);

void monitor_init_thread_support(void){
  thread_data_t *loc;

  csprof_using_threads = 1;

  pthread_once(&iflg,n_init);
  loc = malloc(sizeof(thread_data_t));
  loc->id = thr_c++;
  pthread_setspecific(k,(void *)loc);

  csprof_init_thread_support(loc->id);
}

void csprof_thread_init(killsafe_t *kk, int id);

void *monitor_init_thread(unsigned tid){
  thread_data_t *loc;
  killsafe_t    *safe;

  MSG(1,"mon init thread id = %d, thr_c = %d",tid,thr_c);
  pthread_once(&iflg,n_init);
  loc = malloc(sizeof(thread_data_t));
  loc->id = thr_c++;
  pthread_setspecific(k,(void *)loc);

  safe = (killsafe_t *)malloc(sizeof(killsafe_t));
  csprof_thread_init(safe,loc->id);

  return (void *) safe;
}

void monitor_fini_thread(void *init_thread_data ){

  extern void csprof_thread_fini(csprof_state_t *s);

  csprof_state_t *state = ((killsafe_t *)init_thread_data)->state;

  csprof_thread_fini(state);
}

void monitor_dlopen(const char *path, int flags, void *handle)
{
  csprof_epoch_t *epoch;
  csprof_epoch_lock();

  /* create a new epoch */
  epoch = csprof_epoch_new();

  dl_add_module(path);

  csprof_epoch_unlock();

}

void monitor_dlclose(void *handle) // (const char *library)
{
// we really want the library name
/*
 
  assert(0);

  dl_remove_library(library);
  FIXME: delete intervals from the splay tree too
*/


}


#endif
