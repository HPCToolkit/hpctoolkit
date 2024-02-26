#include <error.h>
#include <math.h>
#include <stdlib.h>
#include <threads.h>

enum {
  N = 1UL<<20,
};

static int work(void* _) {
  double* d_p1 = malloc(N * sizeof d_p1[0]);
  double* d_l1 = malloc(N * sizeof d_l1[0]);
  double* d_r1 = malloc(N * sizeof d_r1[0]);
  for (unsigned long i = 0; i < N; i++) {
    // use transcendental function in the kernel
    d_p1[i] = d_p1[i] + 1.
            + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                  / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
  }
  free(d_p1);
  free(d_l1);
  free(d_r1);
  return 0;
}

static int middle(void* _) {
  thrd_t t;
  if (thrd_create(&t, work, NULL) != thrd_success)
    error(1, 0, "error creating worker thread");
  if(thrd_join(t, NULL) != thrd_success)
    error(1, 0, "error joining worker thread");
  return 0;
}

int main() {
  thrd_t t;
  if (thrd_create(&t, middle, NULL) != thrd_success)
    error(1, 0, "error creating middle thread");
  if(thrd_join(t, NULL) != thrd_success)
    error(1, 0, "error joining middle thread");
  return 0;
}
