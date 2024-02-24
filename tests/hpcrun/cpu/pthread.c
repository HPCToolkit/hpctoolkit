#include <error.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>

enum {
  N = 1ULL<<21,
};

static void* body(void* _) {
  double* d_p1 = malloc(N * sizeof d_p1[0]);
  double* d_l1 = malloc(N * sizeof d_l1[0]);
  double* d_r1 = malloc(N * sizeof d_r1[0]);
  for (unsigned int i = 0; i < N; i++) {
    // use transcendental function in the kernel
    d_p1[i] = d_p1[i] + 1.
            + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                  / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
  }
  free(d_p1);
  free(d_l1);
  free(d_r1);
  return NULL;
}

int main() {
  int err;
  pthread_t threads[3];
  for(int i = 0; i < sizeof threads / sizeof threads[0]; ++i) {
    if((err = pthread_create(&threads[i], NULL, body, NULL)) != 0)
      error(1, err, "error creating thread");
  }
  body(NULL);
  for(int i = 0; i < sizeof threads / sizeof threads[0]; ++i) {
    if((err = pthread_join(threads[i], NULL)) != 0)
      error(1, err, "error joining thread");
  }
  return 0;
}
