#include <math.h>
#include <stdlib.h>
#include <alloca.h>

#define LIMIT_OUTER 100
#define LIMIT 100000

int bar() {
	int x, y;
	x = 1; y = 2;
	return x+y;
}

void foob(double *x){
  *x = (*x) * 3.14 + atan(*x);
}

double moo_leaf(double *x){
  int sz = (drand48() * 10) + 3; // at least 3
  double* mem = (double*)alloca(sz * sizeof(double));
  mem[0] = *x;
  mem[1] = 3.14;
  mem[2] = atan(mem[0]);
  *x = mem[0] * mem[1] + mem[2];
  return *x;
}

void moo_interior(double *x){
  int sz = (drand48() * 10) + 2; // at least 2
  double* mem = (double*)alloca(sz * sizeof(double));
  mem[0] = *x;
  mem[1] = moo_leaf(&mem[0]);
  mem[0] = sin(mem[1]);
  *x = mem[0] + mem[1];
}


int main(int argc, char *argv[]){
  double x,y,z;
  int i,j;

  srand48(5);
  
  x = 2.78;
  z = 3.14;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + sin(y);
      x = log(y) + cos(x);
      moo_interior(&z);
    }
  }
  return 0;
}

