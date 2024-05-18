// SPDX-FileCopyrightText: 2022-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hip/hip_runtime.h>
#include <numeric>
#include <iostream>
#include <vector>

__global__ void vectorAdd(const float* a, const float* b, float* c, int n) {
  int i = blockDim.x * blockIdx.x + threadIdx.x;
  if(i < n) {
    c[i] = a[i] + b[i] + 0.0f;
  }
}

int main() {
  hipError_t err;

  // Check that we have a device to work on
  {
    int nDevices = 0;
    err = hipGetDeviceCount(&nDevices);
    if(err != hipSuccess || nDevices == 0) {
      std::cerr << "No devices available!\n";
      return 77;  // SKIP
    }
  }

  // Allocate and initialize the host-side memory
  std::vector<float> a(5000);
  std::vector<float> b(a.size());
  std::vector<float> c(a.size());
  std::iota(a.begin(), a.end(), 1);
  std::iota(b.begin(), b.end(), 3);

  // Allocate the device-side memory
  float* d_a = nullptr;
  err = hipMalloc((void**)&d_a, a.size() * sizeof(decltype(a)::value_type));
  if(err != hipSuccess) {
    std::cerr << "Failed to allocate d_a: " << hipGetErrorString(err) << "\n";
    return 1;
  }

  float* d_b = nullptr;
  err = hipMalloc((void**)&d_b, b.size() * sizeof(decltype(b)::value_type));
  if(err != hipSuccess) {
    std::cerr << "Failed to allocate d_b: " << hipGetErrorString(err) << "\n";
    return 1;
  }

  float* d_c = nullptr;
  err = hipMalloc((void**)&d_c, c.size() * sizeof(decltype(c)::value_type));
  if(err != hipSuccess) {
    std::cerr << "Failed to allocate d_c: " << hipGetErrorString(err) << "\n";
    return 1;
  }

  // Copy the data in
  err = hipMemcpy(d_a, a.data(), a.size() * sizeof(decltype(a)::value_type), hipMemcpyHostToDevice);
  if(err != hipSuccess) {
    std::cerr << "Failed to memcpy a -> d_a: " << hipGetErrorString(err) << "\n";
    return 1;
  }

  err = hipMemcpy(d_b, b.data(), b.size() * sizeof(decltype(b)::value_type), hipMemcpyHostToDevice);
  if(err != hipSuccess) {
    std::cerr << "Failed to memcpy b -> d_b: " << hipGetErrorString(err) << "\n";
    return 1;
  }

  // Launch the kernel
  int tpb = 256;
  vectorAdd<<<(a.size() + tpb - 1) / tpb, tpb>>>(d_a, d_b, d_c, a.size());

  // Copy the result back out
  err = hipMemcpy(c.data(), d_c, c.size() * sizeof(decltype(c)::value_type), hipMemcpyDeviceToHost);
  if(err != hipSuccess) {
    std::cerr << "Failed to memcpy d_c -> c: " << hipGetErrorString(err) << "\n";
    return 1;
  }

  // Free device memory
  err = hipFree(d_a);
  if(err != hipSuccess) {
    std::cerr << "Failed to free d_a: " << hipGetErrorString(err) << "\n";
  }
  err = hipFree(d_b);
  if(err != hipSuccess) {
    std::cerr << "Failed to free d_b: " << hipGetErrorString(err) << "\n";
  }
  err = hipFree(d_c);
  if(err != hipSuccess) {
    std::cerr << "Failed to free d_c: " << hipGetErrorString(err) << "\n";
  }

  // Validate that the answer is correct
  for(int i = 0; i < c.size(); i++) {
    if(c[i] != 2*i + 4) {
      std::cerr << "Invalid result at c[" << i << "]: expected " << (i+4) << ", got " << c[i] << "\n";
      return 1;
    }
  }

  return 0;
}
