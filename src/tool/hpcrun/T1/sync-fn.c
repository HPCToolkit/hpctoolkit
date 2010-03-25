#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

#include "sync-fn.h"

// void alloc(size_t n) __attribute__ ((weak));
void*
alloc(size_t n)
{
  void* rv    = malloc(n);

  return rv;
}

// extern void r_ret(void* v) __attribute__((weak));
void
r_ret(void* v)
{

  double* a = (double*) v;
  printf("r_ret value = %g\n", a[0]);
}
