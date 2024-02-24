#include <cmath>
#include <omp.h>
#include <vector>
#include <sched.h>

enum {
  Y = 1ULL<<10,
  N = 1ULL<<20,
};

int main() {
#pragma omp parallel num_threads(3)
  {
    std::vector<double> d_p1(N, 0);
    std::vector<double> d_l1(N, 0);
    std::vector<double> d_r1(N, 0);
    for (unsigned int i = 0; i < N; i++) {
      // use transcendental function in the kernel
      d_p1[i] = d_p1[i] + 1.
              + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                    / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
      if (i % Y == 0)
        sched_yield();
    }
  }
  return 0;
}
