// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

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

#include <ucontext.h>
#include <dlfcn.h>

// ***** white box includes *******

#include "cct_metric_data.h"

// ****** external opaque structs ******

typedef struct cct_node_t {
} cct_node_t;

// ****** procedure signature macros ********* 

#define hpcrun_new_metric_s(N) int N(void)
#define hpcrun_get_num_metrics_s(N) int N(void)
#define hpcrun_set_metric_name_s(N) void N(int metric_id, const char* name)
#define hpcrun_sample_callpath_s(N) cct_node_t* N(ucontext_t* ctxt, int metric_id, int metric_inc, int skip, int sync)
#define hpcrun_cct_node_length_s(N) int N(cct_node_t* node)
#define hpcrun_cct_node_update_metric_s(N) void N(cct_node_t* node, int metric_id, metric_bin_fn fn, cct_metric_data_t datum)
#define hpcrun_cct_node_get_metric_s(N) cct_metric_data_t N(cct_node_t* node, int metric_id)

// ****** local function pointers ******

static hpcrun_new_metric_s((*hpcrun_new_metric));
static hpcrun_get_num_metrics_s((*hpcrun_get_num_metrics));
static hpcrun_set_metric_name_s((*hpcrun_set_metric_name));
static hpcrun_sample_callpath_s((*hpcrun_sample_callpath));
static hpcrun_cct_node_length_s((*hpcrun_cct_node_length));
static hpcrun_cct_node_update_metric_s((*hpcrun_cct_node_update_metric));
static hpcrun_cct_node_get_metric_s((*hpcrun_cct_node_get_metric));

typedef cct_node_t* aug_t;

static int alloc_id = -1;
static int r_ret_id = -1;

static
metric_bin_fn_s(repl)
{
  return v2;
}

static
metric_bin_fn_s(max_fn)
{
  if (v2.u >= v1.u){
    return v2;
  }
  return v1;
}

#define APP_MAC(M,A) M(A)

#define FETCH_SYM(n) \
  fn = dlsym(lib, #n);                          \
  if (fn) {                                     \
    printf("ext " #n " symbol found\n");           \
    n = ( APP_MAC(n ## _s,(*)) ) fn; }

static void
fetch_facilities(void)
{
  void* lib = dlopen("libhpcrun.so", RTLD_LAZY);
  void* fn  = NULL;
  if (lib) {
    printf("dlopen worked, looking for symbols\n");

    FETCH_SYM(hpcrun_new_metric);
    FETCH_SYM(hpcrun_get_num_metrics);
    FETCH_SYM(hpcrun_set_metric_name);
    FETCH_SYM(hpcrun_sample_callpath);
    FETCH_SYM(hpcrun_cct_node_length);
    FETCH_SYM(hpcrun_cct_node_update_metric);
    FETCH_SYM(hpcrun_cct_node_get_metric);
  }
}

void
special_metric_init(void)
{
  fetch_facilities();

  int n = hpcrun_get_num_metrics();

  printf("Num metrics = %d\n", n);

  alloc_id = 0;
  r_ret_id = 1;

  hpcrun_set_metric_name(alloc_id, "alloc");
  hpcrun_set_metric_name(r_ret_id, "r_ret");

}

void*
alloc(size_t n)
{
  printf("alloc metric id = %d\n", alloc_id);

  void* rv    = malloc(sizeof(aug_t) + n);
  aug_t* _tmp = (aug_t*) rv;

  ucontext_t context;
  getcontext(&context);
  _tmp[0]     = hpcrun_sample_callpath(&context, alloc_id, 0, 0, 1);
  hpcrun_cct_node_update_metric(_tmp[0], alloc_id, repl,
                                (cct_metric_data_t) (uintptr_t) hpcrun_cct_node_length(_tmp[0]));

  rv          = (void*) &(_tmp[1]);
  return rv;
}

void
r_ret(void* v)
{
  printf("r_ret_id = %d\n", r_ret_id);

  double* a = (double*) v;
  printf("r_ret value = %g\n", a[0]);
  aug_t* _a = ((aug_t*) v) - 1;

  ucontext_t context;
  getcontext(&context);
  cct_node_t* _n = hpcrun_sample_callpath(&context, r_ret_id, 0, 0, 1);
  hpcrun_cct_node_update_metric(*_a, r_ret_id, max_fn,
                                (cct_metric_data_t) (uintptr_t) hpcrun_cct_node_length(_n));

  printf("node a metrics = %d (alloc) || %d (r_ret)\n",
         hpcrun_cct_node_get_metric(*_a, alloc_id).u,
         hpcrun_cct_node_get_metric(*_a, r_ret_id).u);
}

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
  special_metric_init();
  void* thing = handle();
  r_ret(thing);
  printf("Done\n");
}
