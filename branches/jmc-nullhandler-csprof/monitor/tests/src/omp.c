#ifdef _OPENMP
#include <omp.h>
#else
#error "This compiler does not understand OPENMP"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#if defined(USE_PTHREAD_SELF)
#include <pthread.h>
#define TID (unsigned long)pthread_self()
#elif defined(SYS_gettid)
#define TID (unsigned long)syscall(SYS_gettid)
#elif defined(__NR_gettid)
#define TID (unsigned long)syscall(__NR_gettid)
#else
#error "Please define a method to obtain the thread id"
#endif

void Thread(int n)
{
  int i;
  volatile double a = 0.1, b = 1.1, c = 2.1;

  printf("OpenMP Thread %d(TID 0x%lx) of %d: %d iterations\n",omp_get_thread_num(),
	 TID,omp_get_num_threads(),n);
  for (i=0;i<n;i++)
    a += b * c;
}

int main(int argc, char **argv)
{
  omp_set_num_threads(2);
#pragma omp parallel
   {
      Thread(1000000 * (omp_get_thread_num()+1));
   }

   exit(0);
}
