#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

static double accum = 0.;
static const double seed1 = 3.14;
static const double seed2 = 2.78;

static double
x_upd(double x)
{
  return cos(x*x);
}

static double
y_upd(double y)
{
  double t1 = tan(y);
  if (t1 == 0.0) t1 =1.0e-06;
  double t2 = tan(t1);
  //  printf("tan factor = %g\n", t2);
  t2 = pow(abs(t2), .335);
  //  printf("y upd = %g\n", t2);
  return  t2;
}


static double
work3_d(double z)
{
}

static double
work2_d(double x, double y)
{
  double t1, t2, t3;

  t1 = sin(x) + log(y);
  t2 = x + y;
  t3 = sqrt(abs(x*y));

  return t1 * t2 + t3;
}

static void
work1(int in)
{
  static double x = 3.14;
  static double y = 2.78;

  // printf("in = %d, x = %g, y = %g\n", in, x, y);

  for(int i=0; i<in; i++){
    accum += work2_d(x,y);
    x += x_upd(x);
    y -= y_upd(y);
  }
}

static const int NTIMES = 10;

int
main(int argc, char *argv[])
{
  for(int i=0; i < NTIMES; i++) {
    work1(i);
  }
  printf("Ans = %g\n", accum);
}
