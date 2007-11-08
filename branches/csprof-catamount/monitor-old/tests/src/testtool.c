#include <stdio.h>
#include <signal.h>
#include "monitor.h"

void monitor_fini_process(void)
{
  static int f = 0;
  if (f > 0)
    raise(SIGSEGV);
  f++;
  fprintf(stderr,"CALLBACK: monitor_fini_process()\n");
}
void monitor_init_process(char *process, int *argc, char **argv, unsigned pid)
{
  int i;
  fprintf(stderr,"CALLBACK: monitor_init_process(%s,%p,%p,%d): argc=%d, argv=",process,argc,argv,pid,*argc);
  for (i=1;i<*argc;i++)
    fprintf(stderr,"%s ",argv[i]);
  fprintf(stderr,"\n");
}
void monitor_init_library(void)
{
  fprintf(stderr,"CALLBACK: monitor_init_library()\n");
  monitor_opt_error = 0;

#ifndef PROCESS_ONLY
  /* Second test library doesn't monitor errors. */
  monitor_opt_error = MONITOR_NONZERO_EXIT | MONITOR_SIGINT | MONITOR_SIGABRT;
#endif
}
void monitor_fini_library(void)
{
  static int f = 0;
  if (f > 0)
    raise(SIGSEGV);
  f++;
  fprintf(stderr,"CALLBACK: monitor_fini_library()\n");
}
#ifndef PROCESS_ONLY
void monitor_fini_thread(void *ptr)
{
  fprintf(stderr,"CALLBACK: monitor_fini_thread(%p)\n",ptr);
}
void *monitor_init_thread(const unsigned tid)
{
  void *retval = (void *)0xdeadbeef;
  fprintf(stderr,"CALLBACK: monitor_init_thread(0x%x) returns %p\n",tid,retval);
  return(retval);
}
void monitor_init_thread_support(void)
{
  fprintf(stderr,"CALLBACK: monitor_init_thread_support()\n");
}
void monitor_dlopen(const char *library)
{
  fprintf(stderr,"CALLBACK: monitor_init_dlopen(%s)\n",library);
}
#endif
