#include <math.h>
#include <stdlib.h>
#define LIMIT_OUTER 100
#define LIMIT 100000

static int
bar(void)
{
  int x, y;
  x = 1; y = 2;
  return x+y;
}

static void
foob(double *x)
{
  *x = (*x) * 3.14 + atan(*x);
}

void
tfn(void)
{
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
}

