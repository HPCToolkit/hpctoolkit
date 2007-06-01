/* Orion Lawlor's Short UNIX Examples, olawlor@acm.org 2004/3/5

Shows how to use setitimer to get periodic interrupts
for pthreads.
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#ifdef SOLARIS /* needed with at least Solaris 8 */
#include <siginfo.h>
#endif

void printStack(const char *where) {
  printf("%s stack: %p\n",where,&where);
}

void signalHandler(int cause, siginfo_t *HowCome, void *ptr) {
  printStack("signal handler");
  /* printf("   ptr=%p\n",ptr); // what *is* "ptr"? */
}

void *threadStart(void *ptr)
{
  int i;
  struct itimerval itimer;
  
  /* Install our SIGPROF signal handler */
  struct sigaction sa;
  printf("thread sigaction: %p\n",&sa);
  
  sa.sa_sigaction = signalHandler;
  sigemptyset( &sa.sa_mask );
  sa.sa_flags = SA_SIGINFO; /* we want a siginfo_t */
  if (sigaction (SIGPROF, &sa, 0)) {
    perror("sigaction");
    exit(1);
  }
  
  printStack("thread routine");

  /* Request SIGPROF */
  itimer.it_interval.tv_sec=0;
  itimer.it_interval.tv_usec=10*1000;
  itimer.it_value.tv_sec=0;
  itimer.it_value.tv_usec=10*1000;
  setitimer(ITIMER_PROF, &itimer, NULL); 
  
  /* Just wait a bit, which should get a few SIGPROFs */
  for (i=0;i<1000*1000*10;i++) {
  }
  return 0;
}

int main(){ 
  const int nThreads=5;
  int t;
  pthread_t th;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  printStack("main routine");
  for (t=0;t<nThreads;t++)
    pthread_create(&th,&attr,threadStart,NULL);
  pthread_join(th,NULL);
  
  return(0);
}
/*<@>
<@> ******** Program output: ********
<@> main routine stack: 0xbffff280
<@> thread sigaction: 0x4044d9f0
<@> thread routine stack: 0x4044d9e0
<@> thread sigaction: 0x4064e9f0
<@> thread routine stack: 0x4064e9e0
<@> thread sigaction: 0x4084f9f0
<@> thread routine stack: 0x4084f9e0
<@> thread sigaction: 0x40a509f0
<@> thread routine stack: 0x40a509e0
<@> thread sigaction: 0x40c519f0
<@> thread routine stack: 0x40c519e0
<@> */
