#include "general.h"
#include "monitor.h"

#include <linux/unistd.h>
#define M(s) write(2,s"\n",strlen(s)+1)

static int (*the_main)(int, char **, char **);

static int faux_main(int n, char **argv, char **env){
  MSG(1,"calling regular main f faux main");
  return (*the_main)(n,argv,env);
}

void monitor_init_process(struct monitor_start_main_args *m){
  the_main = m->main;
  m->main = &faux_main;
}

void monitor_fini_process(void){
  M("fini process");
}

void monitor_init_library(void){
  extern void csprof_init_internal(void);
  M("monitor init lib calling csprof_init_internal");
  csprof_init_internal();
}

void monitor_fini_library(void){
  extern void csprof_fini_internal(void);
  M("monitor calling csprof_fini_internal");
  csprof_fini_internal();
}

#ifdef CSPROF_THREADS
#include "thread_data.h"
pthread_key_t k;

static int thr_c = 0;
static pthread_once_t iflg = PTHREAD_ONCE_INIT;

pthread_mutex_t mylock;

static void n_init(void){
  int e;
  e = pthread_key_create(&k,free);
}

void monitor_init_thread_support(void){
  thread_data_t *loc;

  pthread_once(&iflg,n_init);
  M("init f initial thread");
  loc = malloc(sizeof(thread_data_t));
  loc->id = -1;

  pthread_setspecific(k,(void *)loc);
  csprof_set_timer();
}

void *monitor_init_thread(unsigned tid ){
  thread_data_t *loc;

  M("Pre fn called");

  pthread_once(&iflg,n_init);
  loc = malloc(sizeof(thread_data_t));
  loc->id = thr_c++;

  pthread_setspecific(k,(void *)loc);

  csprof_set_timer();
  return (void *) loc;
}

void monitor_fini_thread(void *init_thread_data ){
  extern void csprof_pthread_state_fini(void);
  extern void thread_write_data(void);

  M("monitor calling pthread_state_fini");
  /* thread_write_data(); */
  csprof_pthread_state_fini();
}
#endif
