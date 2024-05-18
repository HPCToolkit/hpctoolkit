// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <cmath>
#include <thread>
#include <vector>

static const inline unsigned long N = 1ULL<<20;

static void work() {
  std::vector<double> d_p1(N, 0);
  std::vector<double> d_l1(N, 0);
  std::vector<double> d_r1(N, 0);
  for (unsigned long i = 0; i < N; i++) {
    // use transcendental function in the kernel
    d_p1[i] = d_p1[i] + 1.
            + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                  / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
  }
}

int main() {
  std::thread t([]{
    std::thread t(work);
    t.join();
  });
  t.join();
  return 0;
}
