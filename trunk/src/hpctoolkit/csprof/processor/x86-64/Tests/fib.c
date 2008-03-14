#include <stdio.h>

int fib(int n){
  int rv;

  if (n == 1){
    rv = 1;
  }
  else if (n == 2) {
    rv = 1;
  }
  else {
    rv = fib(n -1) + fib(n -2);
  }
  return rv;
}

int main(int argc, char *argv[]){
  int in = 40;

  int v = fib(in);

  printf("fib(%d) = %d\n",in,v);

  return 0;
}
