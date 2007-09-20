#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#define LIMIT_OUTER 1000
#define LIMIT 1000

double msin(double x){
	int i;
        double rv;
	for(i=0;i<1000;i++) x++;
        rv = x / (x * 1000.);
	return rv;
}

double mcos(double x){
	msin(x);
	return x / (x * 100.);
}


double mlog(double x){
	mcos(x);
	return x/(x * 10.);
}

void foob(double *x){
  *x = (*x) * 3.14 + mlog(*x);
}

int t2_main(int argc, char *argv[]){
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
void t4_foob(double *x){
  *x = (*x) * 3.14 + log(*x);
}

int t4_main(int argc, char *argv[]){
  double x,y;
  int i,j;

  x = 2.78;
  t4_foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + sin(y);
      x = log(y) + cos(x);
    }
  }
  printf("x = %g, y = %g\n",x,y);
  return 0;
}

#define CHECK(pc) do { \
  if (nm_bound(((unsigned long)(pc)),&start,&end)){\
    printf("bound for pc = %p, start = %p, end = %p\n",pc,start,end);\
  }\
  else {\
    printf("NO bounds found for pc = %p\n",pc);\
  }\
} while(0)

extern void _init(void);
extern void _fini(void);

int main(int argc, char *argv[]){
  extern int nm_bound(unsigned long pc, unsigned long *start, unsigned long *end);

  unsigned long start,end;

  CHECK(&mlog + 10);
  CHECK(&t4_main + 9);
  CHECK(&_init - 5);
  CHECK(&_fini);

  return 0;
}
