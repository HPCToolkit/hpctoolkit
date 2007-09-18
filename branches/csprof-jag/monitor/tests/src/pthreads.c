#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

void *Thread(void *arg)
{
  int i, n = *(int *)arg;
  volatile double a = 0.1, b = 1.1, c = 2.1;

  printf("PThread 0x%lx: %d iterations\n",(unsigned long)pthread_self(),n);
  for (i=0;i<n;i++)
    a += b * c;

#if defined(PTHREAD_EXIT)
  pthread_exit(NULL);
#else
  return(NULL);
#endif
}

int main(int argc, char **argv)
{
   pthread_t e_th;
   pthread_t f_th;
   int flops1, flops2, flops3;
   int rc;
   pthread_attr_t attr;

   rc = pthread_attr_init(&attr);
#ifdef PTHREAD_CREATE_UNDETACHED
   rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_UNDETACHED);
#endif
#ifdef PTHREAD_SCOPE_SYSTEM
   rc = pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#endif

   flops1 = 3000000;
   rc = pthread_create(&e_th, &attr, Thread, (void *) &flops1);
   flops2 = 2000000;
   rc = pthread_create(&f_th, &attr, Thread, (void *) &flops2);
   flops3 = 1000000;
   Thread(&flops3);

   rc = pthread_attr_destroy(&attr);
   rc = pthread_join(f_th, NULL);
   rc = pthread_join(e_th, NULL);

   exit(0);
}
