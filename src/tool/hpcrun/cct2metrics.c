//
// cct_node -> metrics map
//
#include <stdbool.h>

#include <hpcrun/metrics.h>
#include <cct/cct.h>
#include <hpcrun/cct2metrics.h>
#include <hpcrun/thread_data.h>

struct cct2metrics_t {
};

//
// ******** Local Data ***********
//
// There is 1 cct2metric map per thread. The public
// interface functions implicitly reference this map
// 

#define tl_map TD_GET(cct2metrics_map)

//
// ******** initialization
//
// The cct2metrics map gets explicit reference
// only at initialization time.
//
// Thereafter, the interface ops refer to it
// implicitly.
// [Singleton pattern for GangOf4 enthusiasts]

void
hpcrun_cct2metrics_init(cct2metrics_t** map)
{
  *map = NULL;
}

// ******** Interface operations **********
// 
//
// for a given cct node, return the metric set
// associated with it.
//
// if there are no metrics for the node, then
// create a metric set, and return it.
//
metric_set_t*
hpcrun_get_metric_set(cct_node_id_t cct_id)
{
  return NULL;
}

//
// check to see if node already has metrics
//
bool
hpcrun_has_metric_set(cct_node_id_t cct_id)
{
  return true;
}
