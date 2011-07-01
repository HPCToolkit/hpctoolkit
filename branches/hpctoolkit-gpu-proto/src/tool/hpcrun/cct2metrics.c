//
// cct_node -> metrics map
//
#include <stdbool.h>

#include <messages/messages.h>
#include <memory/hpcrun-malloc.h>
#include <hpcrun/metrics.h>
#include <cct/cct.h>
#include <hpcrun/cct2metrics.h>
#include <hpcrun/thread_data.h>
#include <lib/prof-lean/splay-macros.h>

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

#define the_map TD_GET(cct2metrics_map)

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
//
// ******* Internal operations: **********
// mapping implemented as a splay tree 
//

static cct2metrics_t*
splay(cct2metrics_t* map, cct_node_id_t node)
{
  REGULAR_SPLAY_TREE(cct2metrics_t, map, node, node, left, right);
  return map;
}

static cct2metrics_t*
cct2metrics_new(cct_node_id_t node, metric_set_t* metrics)
{
  cct2metrics_t* rv = hpcrun_malloc(sizeof(cct2metrics_t));
  rv->node = node;
  rv->metrics = metrics;
  rv->left = rv->right = NULL;
  return rv;
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
// get metric set for a node (NULL return value means no metrics associated).
//
metric_set_t*
hpcrun_get_metric_set(cct_node_id_t cct_id)
{
  cct2metrics_t* map = the_map;
  if (! map) return NULL;

  map = splay(map, cct_id);
  the_map = map;

  if (map->node == cct_id) return map->metrics;
  return NULL;
}

//
// check to see if node already has metrics
//
bool
hpcrun_has_metric_set(cct_node_id_t cct_id)
{
  return (hpcrun_get_metric_set(cct_id) != NULL);
}

//
// associate a metric set with a cct node
//
void
cct2metrics_assoc(cct_node_id_t node, metric_set_t* metrics)
{
  cct2metrics_t* map = the_map;
  if (! map) {
    map = cct2metrics_new(node, metrics);
  }
  else {
    cct2metrics_t* new = cct2metrics_new(node, metrics);
    map = splay(map, node);
    if (map->node == node) {
      EMSG("CCT2METRICS map assoc invariant violated");
    }
    else {
      if (map->node < node) {
        new->left = map->left;
        new->right = map;
        map->left = NULL;
      }
      else {
        new->left = map;
        new->right = map->right;
        map->right = NULL;
      }
      map = new;
    }
  }
  the_map = map;
}
