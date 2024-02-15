#include <cuda.h>
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

  // Allocate and initialize the host-side memory
  std::vector<float> a(5000);
  std::vector<float> b(a.size());
  std::vector<float> c(a.size());
  std::iota(a.begin(), a.end(), 1);
  std::iota(b.begin(), b.end(), 3);

  // Allocate the device-side memory
  float* d_a = nullptr;
  err = cudaMalloc((void**)&d_a, a.size() * sizeof(decltype(a)::value_type));
  if(err != cudaSuccess) {
    std::cerr << "Failed to allocate d_a: " << cudaGetErrorString(err) << "\n";
    return 1;
  }

  float* d_b = nullptr;
  err = cudaMalloc((void**)&d_b, b.size() * sizeof(decltype(b)::value_type));
  if(err != cudaSuccess) {
    std::cerr << "Failed to allocate d_b: " << cudaGetErrorString(err) << "\n";
    return 1;
  }

  float* d_c = nullptr;
  err = cudaMalloc((void**)&d_c, c.size() * sizeof(decltype(c)::value_type));
  if(err != cudaSuccess) {
    std::cerr << "Failed to allocate d_c: " << cudaGetErrorString(err) << "\n";
    return 1;
  }

  // Copy the data in
  err = cudaMemcpy(d_a, a.data(), a.size() * sizeof(decltype(a)::value_type), cudaMemcpyHostToDevice);
  if(err != cudaSuccess) {
    std::cerr << "Failed to memcpy a -> d_a: " << cudaGetErrorString(err) << "\n";
    return 1;
  }

  err = cudaMemcpy(d_b, b.data(), b.size() * sizeof(decltype(b)::value_type), cudaMemcpyHostToDevice);
  if(err != cudaSuccess) {
    std::cerr << "Failed to memcpy b -> d_b: " << cudaGetErrorString(err) << "\n";
    return 1;
  }

  // Launch the kernel
  int tpb = 256;
  vectorAdd<<<(a.size() + tpb - 1) / tpb, tpb>>>(d_a, d_b, d_c, a.size());

  // Copy the result back out
  err = cudaMemcpy(c.data(), d_c, c.size() * sizeof(decltype(c)::value_type), cudaMemcpyDeviceToHost);
  if(err != cudaSuccess) {
    std::cerr << "Failed to memcpy d_c -> c: " << cudaGetErrorString(err) << "\n";
    return 1;
  }

  // Free device memory
  err = cudaFree(d_a);
  if(err != cudaSuccess) {
    std::cerr << "Failed to free d_a: " << cudaGetErrorString(err) << "\n";
    return 1;
  }
  err = cudaFree(d_b);
  if(err != cudaSuccess) {
    std::cerr << "Failed to free d_b: " << cudaGetErrorString(err) << "\n";
    return 1;
  }
  err = cudaFree(d_c);
  if(err != cudaSuccess) {
    std::cerr << "Failed to free d_c: " << cudaGetErrorString(err) << "\n";
    return 1;
  }

  // Validate that the answer is correct
  for(size_t i = 0; i < c.size(); i++) {
    if(c[i] != 2*i + 4) {
      std::cerr << "Invalid result at c[" << i << "]: expected " << (i+4) << ", got " << c[i] << "\n";
      return 1;
    }
  }

  return 0;
}
