#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#define LIMIT_OUTER 1000
#define LIMIT 10000

int bar() {
	int x, y;
	x = 1; y = 2;
	return x+y;
}
void foob(double *x){
  *x = (*x) * 3.14 + atan(*x);
}

int main(int argc, char *argv[]){
  double x,y;
  int i,j;

  
  x = 2.78;
  foob(&x);
  y = 1.00;
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + sin(y);
      x = log(y) + cos(x);
    }
  }
  printf("x = %g, y = %g\n",x,y);
  return 0;
}
