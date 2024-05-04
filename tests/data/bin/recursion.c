// SPDX-FileCopyrightText: 2023-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// The following code implements a "heart" or "double-diamond" recursion pattern: two recursive
// loops sharing a call edge.

__attribute__((noinline))
static void top1(volatile double* x, int i);
__attribute__((noinline))
static void top2(volatile double* x, int i);

__attribute__((noinline))
static void common2(volatile double* x, int i) {
  if (i < 1) {
    x[i] *= 2;
  } else if (i < 3) {
    top1(x, i - 1);
  } else {
    top2(x, i - 1);
  }
}

__attribute__((noinline))
static void common1(volatile double* x, int i) {
  x[i] += 1;
  common2(x, i);
}

__attribute__((noinline))
static void top1(volatile double* x, int i) {
  x[i] *= 3;
  common1(x, i);
}

__attribute__((noinline))
static void top2(volatile double* x, int i) {
  x[i] /= 3;
  common1(x, i);
}

double heart() {
  volatile double x[6] = {1, 2, 3, 4, 5, 6};
  top1(x, 5);
  top2(x, 5);
  return x[0] + x[1] + x[2] + x[3] + x[4] + x[5];
}
