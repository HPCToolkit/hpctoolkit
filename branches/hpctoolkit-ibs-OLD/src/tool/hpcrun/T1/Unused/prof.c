// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

#include <sys/time.h>           /* setitimer() */
#include <signal.h>
#include <sys/signal.h>         /* sigaction() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/unistd.h>

// #include "general.h"

#include "monitor.h"

#define M(s) write(2,s"\n",strlen(s)+1)

pthread_key_t k;

static pthread_once_t iflg = PTHREAD_ONCE_INIT;

pthread_mutex_t mylock;
static struct itimerval itimer;

void monitor_init_library(void){
  int e;
  e = pthread_key_create(&k,free);
}

void init_timer(void){
  itimer.it_value.tv_sec = 0;
  itimer.it_value.tv_usec = 1;

  itimer.it_interval.tv_sec = 0;
  itimer.it_interval.tv_usec = 1;
}

void
set_timer(void)
{
  int ret;

  ret = setitimer(ITIMER_PROF, &itimer, NULL);
  printf("set_timer called, result = %d\n",ret);
}

void init_sigset(sigset_t *ss){
    sigemptyset(ss);
    sigaddset(ss, SIGPROF);
}

void itimer_signal_handler(int sig, siginfo_t *siginfo, void *context){

  int *loc = (int *)pthread_getspecific(k);
  (*loc)++;
  printf("got timer signal f %d\n",pthread_self());
  set_timer();
}

void init_signal_handler(void){
    struct sigaction sa;
    int ret;

    sa.sa_sigaction = itimer_signal_handler;
    //    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    ret = sigaction(SIGPROF, &sa, 0);

    printf("INIT signal handler, result = %d\n",ret);

}

void init_internal(void){
  init_timer();
  init_signal_handler();
  set_timer();

}

static int (*the_main)(int, char **, char **);

static int faux_main(int n, char **argv, char **env){
  int ret;

  printf("calling regular main f faux main\n");
  init_internal();
  asm(".globl monitor_unwind_fence1");
  asm("monitor_unwind_fence1:");
  ret = (*the_main)(n,argv,env);
  asm(".globl monitor_unwind_fence2");
  asm("monitor_unwind_fence2:");

  return ret;
}

void monitor_init_process(struct monitor_start_main_args *m){
  the_main = m->main;
  m->main = &faux_main;
}

void *monitor_init_thread(unsigned tid ){

  int *loc;
  
  loc = (int *) malloc(sizeof(int));
  *loc = 0;
  pthread_setspecific(k,(void *)loc);
  printf("init: thread id %p, loc = %p\n", pthread_self(), pthread_getspecific(k));

  init_internal();

  return (void *)0;
}

void monitor_fini_thread(void *init_thread_data ){

  M("monitor calling pthread_state_fini");
  printf("fini: thread id %p, loc = %p\n", pthread_self(), pthread_getspecific(k));
}
