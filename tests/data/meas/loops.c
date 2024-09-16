// SPDX-FileCopyrightText: 2022-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <errno.h>
#include <error.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

static double clock_diff(struct timespec a, struct timespec b) {
  return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1000000000.0;
}

static struct timespec gettime() {
  struct timespec result;
  if (clock_gettime(CLOCK_MONOTONIC, &result) != 0)
    error(2, errno, "clock_gettime failed");
  return result;
}

static void func1(volatile double* x) {
  for (unsigned int i = 0; i < 1; i++)
    *x = *x * 2 + 3;
  *x = *x * 2 + 3;
}

static void func2(volatile double* x) {
  for (unsigned int i = 0; i < 1; i++) {
    for (unsigned int j = 0; j < 1; j++)
      *x = *x * 2 + 3;
    *x = *x * 2 + 3;
  }
  *x = *x * 2 + 3;
}

__attribute__((always_inline)) static inline void test_separated_loops_helper(volatile double* x) {
  *x = *x * 2 + 3;
}

__attribute__((noinline)) void test_separated_loops(volatile double* x) {
  // The following two loops are arranged such that the compiler maps them to
  // the same line and column numbers in the final debug output.
#line 1000
  for (volatile int i = 0; i < 2; i++)
    test_separated_loops_helper(x);
#line 1000
  for (volatile int i = 0; i < 1; i++)
    test_separated_loops_helper(x);
}

int main() {
#pragma omp parallel num_threads(4)
  {
    struct timespec start = gettime();
    do {
      volatile double x = 2;
      for (unsigned int i = 0; i < 1 << 8; i++) {
        for (unsigned int j = 0; j < 1; j++) {
          for (unsigned int k = 0; k < 1; k++)
            x = x * 2 + 3;
          x = x * 2 + 3;
          func1(&x);
          test_separated_loops(&x);
        }
        for (unsigned int k = 0; k < 1; k++)
          x = x * 2 + 3;
        x = x * 2 + 3;
        func2(&x);
      }
    } while (clock_diff(start, gettime()) < 1.0);
  }
  return 0;
}
