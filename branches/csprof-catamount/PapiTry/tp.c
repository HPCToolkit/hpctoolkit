#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#define LIMIT_OUTER 100
#define LIMIT 100

#include <pthread.h>

// #include <sys/time.h>

double msin(double x){
	int i;
        double xx;

        xx = x;
	for(i=0;i<1000;i++) x++;
	return xx/(xx * 1000.);
}

double mcos(double x){
	msin(x);
	return x/(x * 100.);
}


double mlog(double x){
	mcos(x);
	return x/(x * 10.);
}

void foob(double *x){
#ifndef NO
  *x = (*x) * 3.14 + mlog(*x);
#else
  *x = (*x) * 3.14 + log(*x);
#endif
}

void nobar(void){
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

int bar(){
  double x,y;
  int i,j;

  x = 2.78;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + msin(y);
      x = mlog(y) + mcos(x);
    }
  }
  // printf("x = %g, y = %g\n",x,y);
  return 0;
}
#define NUM_THREADS	3

void *PrintHello(void *threadid)
{
   bar();
   /*   pthread_exit(NULL); */
}

int tp(){
  pthread_t threads[NUM_THREADS];
  int rc, t;

  for(t=0;t<NUM_THREADS;t++){
      printf("Creating thread %d\n", t);
      rc = pthread_create(&threads[t], NULL, PrintHello, (void *)t);
      if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
      }
   }
   printf("Main thread calls bar\n");
   bar();
   printf("Main thread back from bar\n");
   for(t=0;t<NUM_THREADS;t++){
      printf("Joining thread %d\n", t);
      rc = pthread_join(threads[t],0);
      if (rc){
         printf("ERROR; return code from pthread_join() is %d\n", rc);
         exit(-1);
      }
   }
   return 0;
   //   exit(0);
}
