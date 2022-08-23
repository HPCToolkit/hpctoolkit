#include <math.h>
#include <omp.h>

int main() {
#pragma omp parallel num_threads(4)
  {
    double d_p1[30] = {0};
    double d_l1[30] = {0};
    double d_r1[30] = {0};
    for (unsigned int i = 0; i < sizeof d_p1 / sizeof d_p1[0]; i++) {
      // use transcendental function in the kernel
      d_p1[i] = d_p1[i] + 1.
              + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                    / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
    }
  }
  return 0;
}
