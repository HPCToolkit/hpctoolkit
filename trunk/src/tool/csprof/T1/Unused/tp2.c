#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#define LIMIT_OUTER 1000
#define LIMIT 100

#include <pthread.h>
#include <sys/time.h>

double spin(long depth)
{
  int i;
  double sum = 0;
  if (depth > 0) for(i=0;i<10;i++) sum += spin(depth-1);
  return sum;
}

void *pspin(void *t)
{
	int depth = (long) t;
	spin(depth);
	pthread_exit(0);
}

#define NUM_THREADS	1


int main(int argc, char **argv)
{
  pthread_t *threads;
  int t, rc;

  if (argc != 3) {
	printf("usage: threads depth\n");
	exit(-1);
  }
  int nthreads = atoi(argv[1]);
  int depth = atoi(argv[2]);

  threads = malloc(nthreads * sizeof(pthread_t *));

  for(t=0;t<nthreads;t++){
      printf("Creating thread %d\n", t);
      rc = pthread_create(&threads[t], NULL, pspin, (void *)(long) depth);
      if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
      }
   }
   printf("Main thread calls spin\n");
   spin(depth);
   printf("Main thread back from spin\n");
   for(t=0;t<nthreads;t++){
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
