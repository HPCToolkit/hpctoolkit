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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#define LIMIT_OUTER 1000
#define LIMIT 1000
#include <setjmp.h>
#include <ucontext.h>

#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#ifdef SOLARIS /* needed with at least Solaris 8 */
#include <siginfo.h>
#endif
#include <sys/types.h>
#include <libunwind.h>

/* WHY do I need to copy this ???? */

enum
{
  REG_R8 = 0,
# define REG_R8		REG_R8
  REG_R9,
# define REG_R9		REG_R9
  REG_R10,
# define REG_R10	REG_R10
  REG_R11,
# define REG_R11	REG_R11
  REG_R12,
# define REG_R12	REG_R12
  REG_R13,
# define REG_R13	REG_R13
  REG_R14,
# define REG_R14	REG_R14
  REG_R15,
# define REG_R15	REG_R15
  REG_RDI,
# define REG_RDI	REG_RDI
  REG_RSI,
# define REG_RSI	REG_RSI
  REG_RBP,
# define REG_RBP	REG_RBP
  REG_RBX,
# define REG_RBX	REG_RBX
  REG_RDX,
# define REG_RDX	REG_RDX
  REG_RAX,
# define REG_RAX	REG_RAX
  REG_RCX,
# define REG_RCX	REG_RCX
  REG_RSP,
# define REG_RSP	REG_RSP
  REG_RIP,
# define REG_RIP	REG_RIP
  REG_EFL,
# define REG_EFL	REG_EFL
  REG_CSGSFS,		/* Actually short cs, gs, fs, __pad0.  */
# define REG_CSGSFS	REG_CSGSFS
  REG_ERR,
# define REG_ERR	REG_ERR
  REG_TRAPNO,
# define REG_TRAPNO	REG_TRAPNO
  REG_OLDMASK,
# define REG_OLDMASK	REG_OLDMASK
  REG_CR2
# define REG_CR2	REG_CR2
};

pthread_mutex_t mylock;

#define PRINT(buf) fwrite_unlocked(buf,1,n,stderr);
#define PSTR(str) do { \
size_t i = strlen(str); \
fwrite_unlocked(str, 1, i, stderr); \
} while(0)

static inline void MSG(int level, char *format, ...)
{
    va_list args;
    char buf[512];

    va_start(args, format);
    int n;

    pthread_mutex_lock(&mylock);
    flockfile(stderr);
    n = sprintf(buf, "csprof msg [%d][%lx]: ", level, pthread_self());
    PRINT(buf);

    n = vsprintf(buf, format, args);
    PRINT(buf);
    PSTR("\n");
    fflush_unlocked(stderr);
    funlockfile(stderr);
    pthread_mutex_unlock(&mylock);

    va_end(args);
}

pthread_key_t k;
static pthread_once_t iflg = PTHREAD_ONCE_INIT;
typedef struct _td_t {
  int id;
  sigjmp_buf bad_unwind;
} thread_data_t;

void dstr(void *a){
}

static void prepare_itimer(void)
{
  int i;
  struct itimerval itimer;
  extern void set_sigprof(void);
  
  /* set_sigprof(); */

  /* MSG(1,"thread timer init"); */

  /* Request SIGPROF */
  itimer.it_interval.tv_sec=0;
  /*  itimer.it_interval.tv_usec=10*1000; */
  itimer.it_interval.tv_usec=0;
  itimer.it_value.tv_sec=0;
  itimer.it_value.tv_usec=10*1000;
  setitimer(ITIMER_PROF, &itimer, NULL); 
}

void take_sample(void *in,int tid){

  unw_context_t ctx;
  unw_cursor_t cursor;
  
  ucontext_t *uctx = (ucontext_t *) in;
  mcontext_t *mc = &(uctx->uc_mcontext);
  void *pc = (void *)mc->gregs[REG_RIP];

  MSG(1,"%d sample at pc = %lx",tid,pc);

  memcpy(&ctx.uc_mcontext, mc, sizeof(mcontext_t));
  unw_word_t ip;
  int first = 1;

  MSG(1,"before unw_init");
  if(unw_init_local(&cursor, &ctx) < 0) {
    MSG(1,"Could not initialize unwind cursor!");
  }

  for(;;){

    if (unw_get_reg (&cursor, UNW_REG_IP, &ip) < 0){
        MSG(1,"get_reg break");
        break;
    }
    if (first){
      MSG(1,"%d Starting IP = %lp",tid,(void *)ip);
        /*        first = 0; */
    }
    if (unw_step (&cursor) <= 0){
      MSG(1,"Hit unw_step break");
      break;
    }
  }
}

jmp_buf bad_unwind;

void signalHandler(int cause, siginfo_t *HowCome, void *context) {
  /* tid = pthread_self(); */

  thread_data_t *td = (thread_data_t *)pthread_getspecific(k);

  MSG(1,"itimer signal f thread %d",
     td->id);

  if (!sigsetjmp(td->bad_unwind,1)){
    take_sample(context,td->id);
    prepare_itimer();
  }
  else {
    MSG(1,"got bad unwind");
    prepare_itimer();
  }
}

void set_sigprof(void){
  /* Install our SIGPROF signal handler */
  struct sigaction sa;
  MSG(1,"thread sigaction: %p",&sa);
  
  sa.sa_sigaction = signalHandler;
  sigemptyset( &sa.sa_mask );
  sa.sa_flags = SA_SIGINFO; /* we want a siginfo_t */
  if (sigaction (SIGPROF, &sa, 0)) {
    perror("sigaction");
    exit(1);
  }
}

double msin(double x){
	int i;
	for(i=0;i<1000;i++) x++;
	return x;
}

double mcos(double x){
	msin(x);
	return x;
}


double mlog(double x){
	mcos(x);
	return x;
}

void foob(double *x){
#ifdef NO
  *x = (*x) * 3.14 + mlog(*x);
#else
  *x = (*x) * 3.14 + log(*x);
#endif
}

void bar(void){
  double x,y;
  int i,j;

  x = 2.78;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + sin(y);
      x = log(y) + cos(x);
    }
  }
}

int wbar(){
  double x,y;
  int i,j;

  x = 2.78;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + msin(y);
      x = mlog(y) + mcos(x);
#if 0
      void* z = malloc(1); /* ADDED by N.T. */
#endif
    }
  }
  return 0;
}
#include <pthread.h>
#include <stdio.h>
#define NUM_THREADS	3

typedef struct _xptfnt {
  void *(*fn)(void *);
  void *arg_f;
} xpthread_fn_t;


static void n_init(void){
  int e;
  e = pthread_key_create(&k,free);
}

void *doit (void *arg){
  void *rv;
  void *(*ff)(void *);
  void *argf;
  xpthread_fn_t *aa;
  thread_data_t *loc;

  MSG(1,"*** Pre fn called !!! ***");
  pthread_once(&iflg,n_init);


  aa = (xpthread_fn_t *)arg;

  ff   = aa->fn;
  argf = aa->arg_f;

  loc = malloc(sizeof(thread_data_t));
  loc->id = (int)argf;

  pthread_setspecific(k,(void *)loc);
  prepare_itimer();

  rv   = (*ff)(argf);

  free(arg);

  return rv;
}

int xpthread_create(pthread_t *tid,pthread_attr_t *a,
                    void *(*s)(void *),
                    void *arg){
  xpthread_fn_t *the_arg;
  static int first = 1;
  
  the_arg = malloc(sizeof(xpthread_fn_t));
  the_arg->fn    = s;
  the_arg->arg_f = arg;
  pthread_create(tid,a,doit,(void *)the_arg);
  if (first) {
    thread_data_t *loc;

    pthread_once(&iflg,n_init);
    MSG(1,"*** INIT Thread 0!!! ***");

    loc = malloc(sizeof(thread_data_t));
    loc->id = -1;

    pthread_setspecific(k,(void *)loc);
    prepare_itimer();
    first = 0;
    MSG(1,"self is %lx",pthread_self());
    }
}

void *PrintHello(void *threadid)
{
   bar();
   /*   pthread_exit(NULL); */
}

int main()
{
  void csprof_dump_loaded_modules(void);
  void setup_segv(void);

   csprof_dump_loaded_modules();

   pthread_mutex_init(&mylock,NULL);

   set_sigprof();
   setup_segv();

   pthread_t threads[NUM_THREADS];
   int rc, t;
   for(t=0;t<NUM_THREADS;t++){
      printf("Creating thread %d\n", t);
      rc = xpthread_create(&threads[t], NULL, PrintHello, (void *)t);
      if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
      }
   }

   bar();
   for(t=0;t<NUM_THREADS;t++){
      printf("Joining thread %d\n", t);
      rc = pthread_join(threads[t],0);
      if (rc){
         printf("ERROR; return code from pthread_join() is %d\n", rc);
         exit(-1);
      }
   }
   exit(0);
}
