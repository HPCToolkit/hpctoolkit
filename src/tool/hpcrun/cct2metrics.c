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
  metric_data_list_t* kind_metrics;
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
cct2metrics_new(cct_node_id_t node, metric_data_list_t* kind_metrics)
{
  cct2metrics_t* rv = hpcrun_malloc(sizeof(cct2metrics_t));
  rv->node = node;
  rv->kind_metrics = kind_metrics;
  rv->left = rv->right = NULL;
  TMSG(CCT2METRICS, "Node: %p, Metrics: %p", rv->node, rv->kind_metrics);
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

metric_data_list_t*
hpcrun_reify_metric_set(cct_node_id_t cct_id, int metric_id)
{
  TMSG(CCT2METRICS, "REIFY: %p", cct_id);
  metric_data_list_t* rv = hpcrun_get_metric_data_list(cct_id);
  if (rv == NULL) {
    // First time initialze
    TMSG(CCT2METRICS, " -- Metric kind was null, allocating new metric kind");
    rv = hpcrun_new_metric_data_list(metric_id);
    cct2metrics_assoc(cct_id, rv);
  } else {
    rv = hpcrun_reify_metric_data_list_kind(rv, metric_id);
    TMSG(CCT2METRICS, " -- Metric kind found = %p", rv);
  }
  return rv;
}

metric_data_list_t*
hpcrun_get_metric_data_list_specific(cct2metrics_t **map, cct_node_id_t cct_id)
{
  cct2metrics_t *current_map = map ? *map : THREAD_LOCAL_MAP();
  TMSG(CCT2METRICS, "GET_METRIC_SET for %p, using map %p", cct_id, current_map);
  if (! current_map) return NULL;

  current_map = splay(current_map, cct_id);

  if (map)
    *map = current_map;
  else
    THREAD_LOCAL_MAP() = current_map;


  TMSG(CCT2METRICS, " -- After Splay map = %p", cct_id, current_map);

  if (current_map->node == cct_id) {
    TMSG(CCT2METRICS, " -- found %p, returning metrics", current_map->node);
    return current_map->kind_metrics;
  }
  TMSG(CCT2METRICS, " -- cct_id NOT, found. Return NULL");
  return NULL;
}

metric_data_list_t*
hpcrun_get_metric_data_list(cct_node_id_t cct_id)
{
  return hpcrun_get_metric_data_list_specific(NULL, cct_id);
}

metric_data_list_t *
hpcrun_move_metric_data_list_specific(cct2metrics_t **map, cct_node_id_t dest, cct_node_id_t source)
{
  if (dest == NULL || source == NULL) {
    return NULL;
  }

  cct2metrics_t *current_map = map ? *map : THREAD_LOCAL_MAP();
  TMSG(CCT2METRICS, "GET_METRIC_SET for %p, using map %p", source, current_map);
  if (! current_map) return NULL;

  if (map)
    *map = current_map;
  else
    THREAD_LOCAL_MAP() = current_map;
  TMSG(CCT2METRICS, " -- After Splay map = %p", source, current_map);

  if (current_map->node == source) {
    TMSG(CCT2METRICS, " -- found %p, returning metrics", current_map->node);
    metric_data_list_t *metric_data_list = current_map->kind_metrics;
    current_map->kind_metrics = NULL;
    cct2metrics_assoc(dest, metric_data_list); 
    return metric_data_list;
  }
  TMSG(CCT2METRICS, " -- cct_id NOT, found. Return NULL");
  return NULL;
}

metric_data_list_t*
hpcrun_move_metric_data_list(cct_node_id_t dest, cct_node_id_t source)
{
  return hpcrun_move_metric_data_list_specific(NULL, dest, source);
}

//
// associate a metric set with a cct node
//
void
cct2metrics_assoc(cct_node_id_t node, metric_data_list_t* kind_metrics)
{
  cct2metrics_t* map = THREAD_LOCAL_MAP();
  TMSG(CCT2METRICS, "CCT2METRICS_ASSOC for %p, using map %p", node, map);
  if (! map) {
    map = cct2metrics_new(node, kind_metrics);
    TMSG(CCT2METRICS, " -- new map created: %p", map);
  }
  else {
    cct2metrics_t* new = cct2metrics_new(node, kind_metrics);
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


