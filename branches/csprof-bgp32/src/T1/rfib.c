#include <stdio.h>
#include <stdlib.h>

double
fib(double n)
{
  // printf("fib called w %g\n",n);
  if (n > 42.){
    printf("n (= %g) too large ... aborting\n",n);
    abort();
  }
  if (n < 2.0)
    return (1.0);

  return fib(n - 1.) + fib(n - 2.);
}

int
main(int argc, char **argv)
{
  double n;

  if (argc < 2 || sscanf(argv[1], "%lg", &n) < 1)
    n = 10.;

  printf("Input string = %s, input n = %g\n",argv[1],n);
  printf("n = %g, ans = %g\n", n, fib(n));

  return (0);
}
