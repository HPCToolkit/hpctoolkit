// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CL/sycl.hpp>
#include <iostream>
#include <numeric>
#include <vector>

#define SYCL_CALL(statement, msg)                                                      \
  {                                                                                    \
    try {                                                                              \
      statement;                                                                       \
    } catch (sycl::exception const& e) {                                               \
      std::cerr << msg << e.what();                                                    \
      return 1;                                                                        \
    }                                                                                  \
  }

void vectorAdd(const float* a, const float* b, float* c, int i, const int n) {
  if (i < n) {
    c[i] = a[i] + b[i] + 0.0f;
  }
}

int main(int argc, char* argv[]) {
  // Check that we have a device to work on
  sycl::device mygpu;
  try {
    sycl::device testdev{sycl::gpu_selector_v};
    mygpu = testdev;
  } catch (sycl::exception const& e) {
    std::cerr << "No devices available! " << e.what();
    return 77; // SKIP
  }

  // Create default SYCL queue for the GPU
  sycl::queue q(mygpu);

  // Allocate and initialize the host-side memory
  std::vector<float> a(5000);
  std::vector<float> b(a.size());
  std::vector<float> c(a.size());
  std::iota(a.begin(), a.end(), 1);
  std::iota(b.begin(), b.end(), 3);

  // Allocate the device-side memory
  float* d_a = nullptr;
  SYCL_CALL(d_a = sycl::malloc_device<float>(a.size(), q), "Failed to allocate d_a: ");

  float* d_b = nullptr;
  SYCL_CALL(d_b = sycl::malloc_device<float>(b.size(), q), "Failed to allocate d_b: ");

  float* d_c = nullptr;
  SYCL_CALL(d_c = sycl::malloc_device<float>(c.size(), q), "Failed to allocate d_c: ");

  // Copy the data in
  SYCL_CALL(q.memcpy(d_a, a.data(), a.size() * sizeof(decltype(a)::value_type)).wait(),
            "Failed to memcpy a -> d_a: ");
  SYCL_CALL(q.memcpy(d_b, b.data(), b.size() * sizeof(decltype(b)::value_type)).wait(),
            "Failed to memcpy b -> d_b: ");

  // Queue the kernel
  int tpb = 256;
  int N = a.size();
  q.submit([&](sycl::handler& h) {
     h.parallel_for(sycl::range<1>(N), [=](sycl::item<1> item) {
       int i = item.get_linear_id();
       vectorAdd(d_a, d_b, d_c, i, N);
     });
   }).wait();

  // Copy the result back out
  SYCL_CALL(q.memcpy(c.data(), d_c, c.size() * sizeof(decltype(c)::value_type)).wait(),
            "Failed to memcpy d_c -> c: ");

  // Free device memory
  SYCL_CALL(sycl::free(d_a, q), "Failed to free d_a: ");
  SYCL_CALL(sycl::free(d_b, q), "Failed to free d_b: ");
  SYCL_CALL(sycl::free(d_c, q), "Failed to free d_c: ");

  // Validate that the answer is correct
  for (size_t i = 0; i < c.size(); i++) {
    if (c[i] != 2 * i + 4) {
      std::cerr << "Invalid result at c[" << i << "]: expected " << (i + 4) << ", got "
                << c[i] << "\n";
      return 1;
    }
  }

  return 0;
}
