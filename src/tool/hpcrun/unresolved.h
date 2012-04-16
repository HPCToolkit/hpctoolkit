#ifndef UNRESOLVED_H
#define UNRESOLVED_H
#define UNRESOLVED_ROOT -50
#define UNRESOLVED -100

#include <hpcrun/thread_data.h>
#include <cct/cct_bundle.h>
#include <hpcrun/epoch.h>

typedef struct {
  bool tbd;
  uint64_t region_id;
} omp_arg_t;



static inline cct_node_t*
hpcrun_get_tbd_cct(void)
{
  return (TD_GET(epoch)->csdata).unresolved_root;
}

static inline cct_node_t*
hpcrun_get_process_stop_cct(void)
{
  return (TD_GET(epoch)->csdata).tree_root;
}
#endif // UNRESOLVED_H
