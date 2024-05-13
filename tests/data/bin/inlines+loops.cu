// SPDX-FileCopyrightText: 2023-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>

__device__ volatile int unconstant_i = 2;
__device__ volatile int unconstant_j = 2;
__device__ volatile int unconstant_k = 2;
__attribute__((always_inline)) inline
__device__ static void f_inlined() {
  printf("Hello, world!");
}

// 1. Standard function
__global__ void f1() {
  printf("Hello, world!");
}

// 2. Inlined function call
__global__ void f2() {
  f_inlined();
}

// 3. Loops
__global__ void f3_1() {
  for(int i = 0; i < unconstant_i; i++) {
    printf("Hello, world!");
  }
}
__global__ void f3_2() {
  for(int i = 0; i < unconstant_i; i++) {
    for(int j = 0; j < unconstant_j; j++) {
      printf("Hello, world!");
    }
  }
}
__global__ void f3_3() {
  for(int i = 0; i < unconstant_i; i++) {
    for(int j = 0; j < unconstant_j; j++) {
      for(int k = 0; k < unconstant_k; k++) {
        printf("Hello, world!");
      }
    }
  }
}

// 4. Interleaved loops + inlined calls
__attribute__((always_inline)) inline
__device__ static void f4_inline() {
  for(int k = 0; k < unconstant_k; k++) {
    printf("Hello, world!");
  }
}
__global__ void f4_1() {
  for(int i = 0; i < unconstant_i; i++) {
    f4_inline();
  }
}
__global__ void f4_2() {
  for(int i = 0; i < unconstant_i; i++) {
    for(int j = 0; j < unconstant_j; j++) {
      f4_inline();
    }
  }
}
