#include <error.h>
#include <math.h>
#include <stdlib.h>
#include <threads.h>

enum {
  Y = 1ULL<<10,
  N = 1ULL<<21,
};

static int body(void* _) {
  double* d_p1 = malloc(N * sizeof d_p1[0]);
  double* d_l1 = malloc(N * sizeof d_l1[0]);
  double* d_r1 = malloc(N * sizeof d_r1[0]);
  for (unsigned int i = 0; i < N; i++) {
    // use transcendental function in the kernel
    d_p1[i] = d_p1[i] + 1.
            + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                  / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
    if (i % Y == 0)
      thrd_yield();
  }
  free(d_p1);
  free(d_l1);
  free(d_r1);
  return 0;
}

int main() {
  thrd_t threads[2];
  for(int i = 0; i < sizeof threads / sizeof threads[0]; ++i) {
    if(thrd_create(&threads[i], body, NULL) != thrd_success)
      error(1, 0, "error creating thread");
  }
  for(int i = 0; i < sizeof threads / sizeof threads[0]; ++i) {
    if(thrd_join(threads[i], NULL) != thrd_success)
      error(1, 0, "error joining thread");
  }
  return 0;
}
