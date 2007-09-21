#include <stdlib.h>
#include <stdio.h>
#define LIMIT_OUTER 10
#define LIMIT 10

double msin(double x){
	int i;
	for(i=0;i<1000;i++) x++;
	return x / (x * 1000.);
}

double mcos(double x){
	msin(x);
	return x / (x * 100.);
}


double mlog(double x){
	mcos(x);
	return x / (x * 10.);
}

void foob(double *x){
  *x = (*x) * 3.14 + mlog(*x);
}

int main(int argc, char *argv[]){
  double x,y;
  int i,j;

  x = 2.78;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + msin(y);
      x = mlog(y) + mcos(x);
    }
  }
  printf("x = %g, y = %g\n",x,y);
  return 0;
}
