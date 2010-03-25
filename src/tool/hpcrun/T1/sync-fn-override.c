#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

#include <ucontext.h>
#include <dlfcn.h>

#include "sync-fn.h"

#ifdef NO

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
#else

typedef struct {
  void* addr;
  size_t nnn;
} _info_t;

typedef _info_t aug_t;

void*
alloc(size_t n)
{
  
  fprintf(stderr, "--override alloc called!\n");
  void* rv    = malloc(sizeof(aug_t) + n);
  aug_t* _tmp = (aug_t*) rv;

  _tmp->addr = (void*) alloc;
  _tmp->nnn  = n;

  rv          = (void*) &(_tmp[1]);
  return rv;
}

void
r_ret(void* v)
{
  fprintf(stderr, "--override r_ret called!\n");
  double* a = (double*) v;
  printf("r_ret value = %g\n", a[0]);
  aug_t* _a = ((aug_t*) v) - 1;
}
#endif // NO
