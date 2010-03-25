//
// This example illustrates a method for constructing custom, synchronous metrics
// using libhpcrun facilities.
//
// By using dynamic loading, the need to build a custom libhpcrun.so is eliminated.
// This example further shows how to make *introspective* metrics.
//
// NB: an introspective metric is a metric that depends on some property of the backtrace.
//     This example uses the the length of the backtrace at the sample point as the illustrated metric
//
// A final noteworthy feature of this example is the pairing of sampled functions: The alloc function in
// this example is paired with the r_ret function. To accomplish this pairing, the alloc function (the function called
// first) must augment it's normal return value with enough space to hold a cct_node. When the alloc function is
// invoked, the backtrace is generated, and the cct_node representing the backtrace is stored in the slot designated for it
// in the augmented return value. When r_ret is called with arbitrary alloc return value, the stored cct_node
// identifies the calling context for that alloc value. So, the metric computed by r_ret is stored in the
// cct_node associated with the alloc call.
//
// To make this custom metric work, you will need to invoke hpcrun with the event 'SYNC@2' to insure that
// 2 synchronous metric slots are allocated in the cct. Sample invocation:
//
//     hpcrun -e SYNC@2 custom-metrics
//

// ******** system includes **********

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

// ******** local includes **********

#include "sync-fn.h"

void*
handle(void)
{
  void* rv = alloc(100);
  double* secret_array = (double*) rv;
  secret_array[0] = sin(3.13);
  return rv;
}

int
main(int argc, char* argv[])
{
  void* thing = handle();
  r_ret(thing);
  printf("Done\n");
}
