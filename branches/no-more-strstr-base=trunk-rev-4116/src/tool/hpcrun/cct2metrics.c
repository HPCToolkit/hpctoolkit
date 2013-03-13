//
// cct_node -> metrics map
//
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

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
  struct cct2metrics_t* right;
  struct cct2metrics_t* left;
};



//
// ******** Local Data ***********
//
// There is 1 cct2metric map per thread. The public
// interface functions implicitly reference this map
// 

#define THREAD_LOCAL_MAP() TD_GET(core_profile_trace_data.cct2metrics_map)

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
  TMSG(CCT2METRICS, "Init, map = %p", *map);
  *map = NULL;
}
//
// ******* Internal operations: **********
// mapping implemented as a splay tree 
//

void
help_splay_tree_dump(cct2metrics_t* tree, int indent)
{
  if (! tree) return;

  help_splay_tree_dump(tree->right, indent+2);

  char buf[300];
  for (int i=0; i<indent; i++) buf[i] = ' ';
  buf[indent] = '\0';
  snprintf(buf+indent, sizeof(buf)-(indent+1), "%p", tree->node);
  TMSG(CCT2METRICS, "%s", buf);

  help_splay_tree_dump(tree->left, indent+2);
}

void
splay_tree_dump(cct2metrics_t* map)
{
  TMSG(CCT2METRICS, "Splay tree %p appears below", map);
  help_splay_tree_dump(map, 0);
}


static cct2metrics_t*
splay(cct2metrics_t* map, cct_node_id_t node)
{
  TMSG(CCT2METRICS, "splay map = %p, node = %p", map, node);
  REGULAR_SPLAY_TREE(cct2metrics_t, map, node, node, left, right);
  TMSG(CCT2METRICS, "new map = %p, top node = %p", map, map->node);
  return map;
}

static cct2metrics_t*
cct2metrics_new(cct_node_id_t node, metric_set_t* metrics)
{
  cct2metrics_t* rv = hpcrun_malloc(sizeof(cct2metrics_t));
  rv->node = node;
  rv->metrics = metrics;
  rv->left = rv->right = NULL;
  TMSG(CCT2METRICS, "Node: %p, Metrics: %p", rv->node, rv->metrics);
  return rv;
}
// ******** Interface operations **********
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
  TMSG(CCT2METRICS, "REIFY: %p", cct_id);
  metric_set_t* rv = hpcrun_get_metric_set(cct_id);
  TMSG(CCT2METRICS, " -- Metric set found = %p", rv);
  if (rv) return rv;
  TMSG(CCT2METRICS, " -- Metric set was null, allocating new metric set");
  cct2metrics_assoc(cct_id, rv = hpcrun_metric_set_new());
  TMSG(CCT2METRICS, "REIFY returns %p", rv);
  return rv;
}

//
// get metric set for a node (NULL return value means no metrics associated).
//
metric_set_t*
hpcrun_get_metric_set(cct_node_id_t cct_id)
{
  cct2metrics_t* map = THREAD_LOCAL_MAP();
  TMSG(CCT2METRICS, "GET_METRIC_SET for %p, using map %p", cct_id, map);
  if (! map) return NULL;

  map = splay(map, cct_id);
  THREAD_LOCAL_MAP() = map;
  TMSG(CCT2METRICS, " -- After Splay map = %p", cct_id, map);

  if (map->node == cct_id) {
    TMSG(CCT2METRICS, " -- found %p, returning metrics", map->node);
    return map->metrics;
  }
  TMSG(CCT2METRICS, " -- cct_id NOT, found. Return NULL");
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
  cct2metrics_t* map = THREAD_LOCAL_MAP();
  TMSG(CCT2METRICS, "CCT2METRICS_ASSOC for %p, using map %p", node, map);
  if (! map) {
    map = cct2metrics_new(node, metrics);
    TMSG(CCT2METRICS, " -- new map created: %p", map);
  }
  else {
    cct2metrics_t* new = cct2metrics_new(node, metrics);
    map = splay(map, node);
    TMSG(CCT2METRICS, " -- map after splay = %p,"
         " node sought = %p, mapnode = %p", map, node, map->node);
    if (map->node == node) {
      EMSG("CCT2METRICS map assoc invariant violated");
    }
    else {
      if (map->node < node) {
        TMSG(CCT2METRICS, " -- less-than insert %p < %p", map->node, node);
        new->left = map;
        new->right = map->right;
        map->right = NULL;
      }
      else {
        TMSG(CCT2METRICS, " -- greater-than insert %p > %p", map->node, node);
        new->left = map->left;
        new->right = map;
        map->left = NULL;
      }
      map = new;
      TMSG(CCT2METRICS, " -- new map after insertion %p.(%p, %p)", map->node, map->left, map->right);
    }
  }
  THREAD_LOCAL_MAP() = map;
  TMSG(CCT2METRICS, "METRICS_ASSOC final, THREAD_LOCAL_MAP = %p", THREAD_LOCAL_MAP());
  if (ENABLED(CCT2METRICS)) splay_tree_dump(THREAD_LOCAL_MAP());
}
