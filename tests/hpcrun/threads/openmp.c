// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <math.h>
#include <omp.h>
#include <stdlib.h>

enum {
  N = 1UL<<20,
};

int main() {
  omp_set_nested(1);
  #pragma omp parallel num_threads(2)
  {
    if (omp_get_thread_num() == 1) {
      #pragma omp parallel num_threads(2)
      {
        if (omp_get_thread_num() == 1) {
          double* d_p1 = malloc(N * sizeof d_p1[0]);
          double* d_l1 = malloc(N * sizeof d_l1[0]);
          double* d_r1 = malloc(N * sizeof d_r1[0]);
          for (unsigned long i = 0; i < N; i++) {
            // use transcendental function in the kernel
            d_p1[i] = d_p1[i] + 1.
                    + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                          / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
          }
          free(d_p1);
          free(d_l1);
          free(d_r1);
        }
      }
    }
  }
  return 0;
}
