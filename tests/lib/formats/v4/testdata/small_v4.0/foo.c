

static void foo() {
  volatile double x = 2;
  for(int i = 0; i < 1<<27; i++) x = x * 2 + 3;
}

static void bar() {
  foo();
}

int main() {
  foo(); bar();
}

/*
  Steps to generate test data:
  $ cp foo.c /tmp/foo.c
  $ gcc -o /tmp/foo /tmp/foo.c -g
  # Events are tuned to generate 2-3 cycles samples. Check logs to verify.
  $ hpcrun -o m.foo -e cycles@1000000000 -e instructions@100000000000 -t /tmp/foo
  $ hpcstruct m.foo
  $ hpcprof -o d.foo m.foo -M stats
*/
