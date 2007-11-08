#include <pthread.h>
#include <stdlib.h>

#include "monitor.h"

#include <linux/unistd.h>
#define M(s) write(2,s"\n",strlen(s)+1)

static int (*the_main)(int, char **, char **);

static int faux_main(int n, char **argv, char **env){
  M("calling regular main f faux main");
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
  M("monitor init lib happening");
}

void monitor_fini_library(void){
  M("monitor fini lib happening");
}

#include "thread_data.h"
pthread_key_t k;

static int thr_c = 0;
static pthread_once_t iflg = PTHREAD_ONCE_INIT;

pthread_mutex_t mylock;
int csprof_using_threads = 0;

static void n_init(void){
  int e;
  e = pthread_key_create(&k,free);
}

void monitor_init_thread_support(void){
  thread_data_t *loc;

  csprof_using_threads = 1;
  pthread_once(&iflg,n_init);
  M("init f initial thread");
  loc = malloc(sizeof(thread_data_t));
  loc->id = -1;

  pthread_setspecific(k,(void *)loc);
}

void *monitor_init_thread(unsigned tid ){
  thread_data_t *loc;

  M("Pre fn called");

  pthread_once(&iflg,n_init);
  loc = malloc(sizeof(thread_data_t));
  loc->id = thr_c++;

  pthread_setspecific(k,(void *)loc);

  return (void *) loc;
}

void monitor_fini_thread(void *init_thread_data ){

  M("monitor calling fini thread");
}
