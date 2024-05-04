// SPDX-FileCopyrightText: 2023-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>

volatile int unconstant_i = 2;
volatile int unconstant_j = 2;
volatile int unconstant_k = 2;
__attribute__((always_inline)) inline
static void f_inlined() {
  printf("Hello, world!");
}

// 1. Standard function
void f1() {
  printf("Hello, world!");
}

// 2. Inlined function call
void f2() {
  f_inlined();
}

// 3. Loops
void f3_1() {
  for(int i = 0; i < unconstant_i; i++) {
    printf("Hello, world!");
  }
}
void f3_2() {
  for(int i = 0; i < unconstant_i; i++) {
    for(int j = 0; j < unconstant_j; j++) {
      printf("Hello, world!");
    }
  }
}
void f3_3() {
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
static void f4_inline() {
  for(int k = 0; k < unconstant_k; k++) {
    printf("Hello, world!");
  }
}
void f4_1() {
  for(int i = 0; i < unconstant_i; i++) {
    f4_inline();
  }
}
void f4_2() {
  for(int i = 0; i < unconstant_i; i++) {
    for(int j = 0; j < unconstant_j; j++) {
      f4_inline();
    }
  }
}
