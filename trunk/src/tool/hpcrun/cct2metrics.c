//
// cct_node -> metrics map
//
#include <stdbool.h>

#include <hpcrun/metrics.h>
#include <cct/cct.h>
#include <hpcrun/cct2metrics.h>
#include <hpcrun/thread_data.h>
#include <lib/prof-lean/splay-macros.h>
#include <messages/messages.h>

//
// ***** The splay tree node *****
//

struct cct2metrics_t {
  cct_node_id_t node;
  metric_set_t* metrics;
  //
  // left and right pointers for splay tree of siblings
  //
  cct2metrics_t* right;
  cct2metrics_t* left;
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

// for a given cct node, see if there is a mapping
// from that node to a set of metrics.
// That is, check the 
//

// for a given cct node, return the metric set
// associated with it.
//
// if there are no metrics for the node, then
// create a metric set, and return it.
//

metric_set_t*
hpcrun_reify_metric_set(cct_node_id_t cct_id)
{
  metric_set_t* rv = hpcrun_get_metric_set(cct_id);
  if (rv) return rv;
  cct2metrics_assoc(cct_id, rv = hpcrun_metric_set_new());
  return rv;
}

//
// get metric set for a node (NULL value is ok).
//
metric_set_t*
hpcrun_get_metric_set(cct_node_id_t cct_id)
{
  return (metric_set_t*) hpcrun_cct_metrics((cct_node_t*)cct_id);
}

//
// check to see if node already has metrics
//
bool
hpcrun_has_metric_set(cct_node_id_t cct_id)
{
  return (hpcrun_cct_metrics(cct_id) != NULL);
}

// *** TEMPORARY
extern void cct_node_metrics_sb(cct_node_t* node, metric_set_t* metrics);

//
// associate a metric set with a cct node
//
void
cct2metrics_assoc(cct_node_id_t node, metric_set_t* metrics)
{
  cct_node_metrics_sb(node, metrics);
}
