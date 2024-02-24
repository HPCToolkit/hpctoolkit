#include <cmath>
#include <thread>
#include <vector>

enum {
  N = 1ULL<<21,
};

static void body() {
  std::vector<double> d_p1(N, 0);
  std::vector<double> d_l1(N, 0);
  std::vector<double> d_r1(N, 0);
  for (unsigned int i = 0; i < N; i++) {
    // use transcendental function in the kernel
    d_p1[i] = d_p1[i] + 1.
            + (sqrt(exp(log(d_l1[i] * d_l1[i])) + exp(log(d_r1[i] * d_r1[i]))))
                  / (sqrt(exp(log(d_l1[i] * d_r1[i])) + exp(log((d_r1[i] * d_l1[i])))));
  }
}

int main() {
  std::vector<std::thread> threads(3);
  for(std::thread& t: threads)
    t = std::thread(body);
  body();
  for(std::thread& t: threads)
    t.join();
  return 0;
}
