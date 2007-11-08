#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

void *Thread(void *arg)
{
  int i, n = *(int *)arg;
  volatile double a = 0.1, b = 1.1, c = 2.1;

  if (n == 1000000)
    {
      printf("PThread 0x%lx: loop of %d iterations\n",(unsigned long)pthread_self(),n);
      for (i=0;i<n;i++)
	{
	  a += b * c;
	}
    }
  else
    {
      printf("PThread 0x%lx: inf. loop of %d iterations\n",(unsigned long)pthread_self(),n);
      while(1)
	{
	  for (i=0;i<n;i++)
	    {
	      a += b * c;
	    }
	}
    }
}

int main(int argc, char **argv)
{
   pthread_t e_th;
   pthread_t f_th;
   int rc, flops1, flops2, flops3;

#if defined(CANCEL)
   int oldtype;
   rc = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldtype);
#if defined(CANCEL_ASYNC)
   rc = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
#endif
#endif

   flops1 = 3000000;
   rc = pthread_create(&e_th, NULL, Thread, (void *) &flops1);
   flops2 = 2000000;
   rc = pthread_create(&f_th, NULL, Thread, (void *) &flops2);
   flops3 = 1000000;
   Thread(&flops3);

#if defined(CANCEL)
   rc = pthread_cancel(e_th);
   rc = pthread_cancel(f_th);
#endif
   while(1) sched_yield();
}
