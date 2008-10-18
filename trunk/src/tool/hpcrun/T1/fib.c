#include <stdio.h>

long
fib(int n)
{
  if (n < 2)
    return (1);

  return fib(n - 1) + fib(n - 2);
}

int
main(int argc, char **argv)
{
  int n;

  if (argc < 2 || sscanf(argv[1], "%d", &n) < 1)
    n = 10;

  printf("n = %d, ans = %ld\n", n, fib(n));

  return (0);
}
