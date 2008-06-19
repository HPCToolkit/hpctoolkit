#include <math.h>
#include <stdlib.h>
#define LIMIT_OUTER 100
#define LIMIT 100000

void foob(double *x){
  *x = (*x) * 3.14 + atan(*x);
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
#if 0
      void* z = malloc(1); /* ADDED by N.T. */
#endif
    }
  }
  return 0;
}

