#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

volatile double a = 0.1, b = 1.1, c = 2.1;

int main(int argc, char **argv)
{
  int i;
  printf("Doing 100000000 iters. of a += b * c on doubles.\n");
  for (i=0;i<100000000;i++)
    a += b * c;
}
