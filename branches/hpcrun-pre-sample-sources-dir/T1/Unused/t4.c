#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#define LIMIT_OUTER 1000
#define LIMIT 1000

void foob(double *x){
  *x = (*x) * 3.14 + log(*x);
}

int main(int argc, char *argv[]){
  double x,y;
  int i,j;

  x = 2.78;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + sin(y);
      x = log(y) + cos(x);
    }
  }
  printf("x = %g, y = %g\n",x,y);
  return 0;
}


