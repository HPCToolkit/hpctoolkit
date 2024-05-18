// SPDX-FileCopyrightText: 2023-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <cuda.h>
#include <iostream>

// The following code implements a "heart" or "double-diamond" recursion pattern: two recursive
// loops sharing a call edge.

__attribute__((noinline))
__device__ static void top1(volatile double* x, int i);
__attribute__((noinline))
__device__ static void top2(volatile double* x, int i);

__attribute__((noinline))
__device__ static void common2(volatile double* x, int i) {
  if (i < 1) {
    x[i] *= 2;
  } else if (i < 3) {
    top1(x, i - 1);
  } else {
    top2(x, i - 1);
  }
}

__attribute__((noinline))
__device__ static void common1(volatile double* x, int i) {
  x[i] += 1;
  common2(x, i);
}

__attribute__((noinline))
__device__ static void top1(volatile double* x, int i) {
  x[i] *= 3;
  common1(x, i);
}

__attribute__((noinline))
__device__ static void top2(volatile double* x, int i) {
  x[i] /= 3;
  common1(x, i);
}

__attribute__((noinline))
__device__ static double heart() {
  volatile double x[6] = {1, 2, 3, 4, 5, 6};
  top1(x, 5);
  top2(x, 5);
  return x[0] + x[1] + x[2] + x[3] + x[4] + x[5] + x[6];
}

// Main kernel
__global__ static void kernmain() {
  heart();
}

int main() {
  cudaError_t err;

  // Check that we have a device to work on
  {
    int nDevices = 0;
    err = cudaGetDeviceCount(&nDevices);
    if(err != cudaSuccess || nDevices == 0) {
      std::cerr << "No devices available!\n";
      return 77;  // SKIP
    }
  }

  for(int i = 0; i < 100; i++) {
    // Launch the kernel
    kernmain<<<100, 32>>>();
    err = cudaGetLastError();
    if(err != cudaSuccess) {
      std::cerr << "Error during kernel launch\n";
      return 1;
    }

    // Wait for the kernel to complete
    err = cudaDeviceSynchronize();
    if(err != cudaSuccess) {
      std::cerr << "Error returned by kernel\n";
      return 1;
    }
  }

  return 0;
}
