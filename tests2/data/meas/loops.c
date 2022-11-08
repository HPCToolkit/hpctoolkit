#include <math.h>
#include <time.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>

static double clock_diff(struct timespec a, struct timespec b) {
  return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1000000000.0;
}

static struct timespec gettime() {
  struct timespec result;
  if(clock_gettime(CLOCK_MONOTONIC, &result) != 0)
    error(2, errno, "clock_gettime failed");
  return result;
}

static void func1(volatile double* x) {
  for(unsigned int i = 0; i < 1; i++)
    *x = *x * 2 + 3;
  *x = *x * 2 + 3;
}

static void func2(volatile double* x) {
  for(unsigned int i = 0; i < 1; i++) {
    for(unsigned int j = 0; j < 1; j++)
      *x = *x * 2 + 3;
    *x = *x * 2 + 3;
  }
  *x = *x * 2 + 3;
}

int main() {
#pragma omp parallel num_threads(4)
  {
    struct timespec start = gettime();
    do {
      volatile double x = 2;
      for(unsigned int i = 0; i < 1<<8; i++) {
        for(unsigned int j = 0; j < 1; j++) {
          for(unsigned int k = 0; k < 1; k++)
            x = x * 2 + 3;
          x = x * 2 + 3;
          func1(&x);
        }
        for(unsigned int k = 0; k < 1; k++)
          x = x * 2 + 3;
        x = x * 2 + 3;
        func2(&x);
      }
    } while(clock_diff(start, gettime()) < 1.0);
  }
  return 0;
}
