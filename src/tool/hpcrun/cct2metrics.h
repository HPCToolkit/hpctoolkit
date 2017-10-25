#ifndef CCT2METRICS_H
#define CCT2METRICS_H

//
// cct_node -> metrics map
//
#include <stdbool.h>

#include <hpcrun/metrics.h>
#include <cct/cct.h>


//
// ******** Typedef ********
//
//
typedef struct cct2metrics_t cct2metrics_t;
//
// ******** initialization
//
// The cct2metrics map gets explicit reference
// only at initialization time.
//
// Thereafter, the interface ops refer to it
// implicitly.
// [Singleton pattern for GangOf4 enthusiasts]

extern void hpcrun_cct2metrics_init(cct2metrics_t** map);

// ******** Interface operations **********
// 

//
// for a given cct node, return the metric set
// associated with it.
//
// if there are no metrics for the node, then
// create a metric set, and return it.
//
extern metric_set_t* hpcrun_reify_metric_set(cct_node_id_t cct_id);

//
// get metric data list for a node (NULL value is ok).
//
extern metric_data_list_t* hpcrun_get_metric_data_list(cct_node_id_t cct_id);

//
// get metric set for a node (NULL value is ok).
//
extern metric_set_t* hpcrun_get_metric_set(cct_node_id_t cct_id);

//
// check to see if node already has metrics
//
extern bool hpcrun_has_metric_set(cct_node_id_t cct_id);

extern metric_data_list_t *hpcrun_new_metric_data_list();

extern void cct2metrics_assoc(cct_node_t* node, metric_data_list_t* kind_metrics);

//extern cct2metrics_t* cct2metrics_new(cct_node_id_t node, metric_set_t** kind_metrics);

static inline void
cct_metric_data_increment(int metric_id,
			  cct_node_t* x,
			  cct_metric_data_t incr)
{
  metric_set_t *set = hpcrun_reify_metric_set(x);
  hpcrun_metric_std_inc(metric_id, set, incr);
}



#endif // CCT2METRICS_H
