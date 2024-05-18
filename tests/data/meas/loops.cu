// SPDX-FileCopyrightText: 2022-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include <cuda.h>
#include <iostream>

__device__ static void func1(volatile double* x) {
  for(unsigned int i = 0; i < 1; i++)
    *x = *x * 2 + 3;
  *x = *x * 2 + 3;
}

__device__ static void func2(volatile double* x) {
  for(unsigned int i = 0; i < 1; i++) {
    for(unsigned int j = 0; j < 1; j++)
      *x = *x * 2 + 3;
    *x = *x * 2 + 3;
  }
  *x = *x * 2 + 3;
}

__global__ static void kernmain() {
  volatile double x = 2;
  for(unsigned int i = 0; i < 1<<12; i++) {
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
