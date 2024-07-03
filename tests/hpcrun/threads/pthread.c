// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <error.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

enum {
  N = 1UL << 20,
};

static void* work(void* _) {
  double* d_p1 = malloc(N * sizeof d_p1[0]);
  double* d_l1 = malloc(N * sizeof d_l1[0]);
  double* d_r1 = malloc(N * sizeof d_r1[0]);
  for (unsigned long i = 0; i < N; i++) {
    // use transcendental function in the kernel
    d_p1[i] = d_p1[i] + 1. +
              (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i])))) /
                  (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
  }
  free(d_p1);
  free(d_l1);
  free(d_r1);
  return NULL;
}

static void* middle(void* _) {
  pthread_t t;
  int err;
  if ((err = pthread_create(&t, NULL, work, NULL)) != 0)
    error(1, err, "error creating worker thread");
  if ((err = pthread_join(t, NULL)) != 0) error(1, err, "error joining worker thread");
  return NULL;
}

int main() {
  pthread_t t;
  int err;
  if ((err = pthread_create(&t, NULL, middle, NULL)) != 0)
    error(1, err, "error creating middle thread");
  if ((err = pthread_join(t, NULL)) != 0) error(1, err, "error joining middle thread");
  return 0;
}
