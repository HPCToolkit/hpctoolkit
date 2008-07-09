#include <stdio.h>
#include <stdlib.h>

#include <math.h>

#include "papi-simple.h"
#include "simple-handler.h"

// ************************* Cycle burner ********************


#define LIMIT_OUTER 100
#define LIMIT 100000

void foob(double *x){
  *x = (*x) * 3.14 + atan(*x);
}

double cycles(void)
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
  return x+y;
}

// *************************** Main program ***************

int main(int argc, char *argv[])
{
#include "simple-handler.h"

  //  papi_skip_thread_init(argc > 1);

  papi_setup(simple_handler);

  double result = cycles();

  papi_over();

  printf("result = %g\n",result);
  printf("count = %d\n",simple_papi_count());
  return 0;
}
