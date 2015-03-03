/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "ompt-region.h"
#include "ompt-defer.h"
#include "ompt-callback.h"
#include "ompt-callstack.h"

#include "ompt-interface.h"
#include "ompt-region-map.h"

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>
#include <hpcrun/unresolved.h>




/******************************************************************************
 * external declarations 
 *****************************************************************************/

extern int omp_get_level(void);
extern int omp_get_thread_num(void);



/******************************************************************************
 * private operations
 *****************************************************************************/

//
// TODO: add trace correction info here
// FIXME: merge metrics belongs in a different file. it is not specific to 
// OpenMP
//
static void
merge_metrics(cct_node_t *a, cct_node_t *b, merge_op_arg_t arg)
{
  // if nodes a and b are the same, no need to merge
  if (a == b) return;

  metric_set_t* mset_a = hpcrun_get_metric_set(a);
  metric_set_t* mset_b = hpcrun_get_metric_set(b);

  if (!mset_a || !mset_b) return;

  int num_metrics = hpcrun_get_num_metrics();
  for (int i = 0; i < num_metrics; i++) {
    cct_metric_data_t *mdata_a = hpcrun_metric_set_loc(mset_a, i);
    cct_metric_data_t *mdata_b = hpcrun_metric_set_loc(mset_b, i);

    // FIXME: this test depends upon dense metric sets. sparse metrics
    //        should ensure that node a has the result
    if (!mdata_a || !mdata_b) continue;

    metric_desc_t *mdesc = hpcrun_id2metric(i);
    switch(mdesc->flags.fields.valFmt) {
    case MetricFlags_ValFmt_Int:  mdata_a->i += mdata_b->i; break;
    case MetricFlags_ValFmt_Real: mdata_a->r += mdata_b->r; break;
    default:
      TMSG(DEFER_CTXT, "in merge_op: what's the metric type");
      monitor_real_exit(1);
    }
  }
}


static void
omp_resolve(cct_node_t* cct, cct_op_arg_t a, size_t l)
{
  cct_node_t *prefix;
  thread_data_t *td = (thread_data_t *)a;
  uint64_t my_region_id = (uint64_t)hpcrun_cct_addr(cct)->ip_norm.lm_ip;
  TMSG(DEFER_CTXT, "omp_resolve: try to resolve region 0x%lx", my_region_id);
  uint64_t partial_region_id = 0;
  if ((prefix = hpcrun_region_lookup(my_region_id))) {
    TMSG(DEFER_CTXT, "omp_resolve: resolve region 0x%lx to 0x%lx", my_region_id, is_partial_resolve(prefix));
    // delete cct from its original parent before merging
    hpcrun_cct_delete_self(cct);
    TMSG(DEFER_CTXT, "omp_resolve: delete from the tbd region 0x%lx", hpcrun_cct_addr(cct)->ip_norm.lm_ip);

    partial_region_id = is_partial_resolve(prefix);

    if (partial_region_id == 0) {
#if 1
      cct_node_t *root = region_root(prefix);
      prefix = hpcrun_cct_insert_path_return_leaf(root,  prefix);
#else
      prefix = hpcrun_cct_insert_path_return_leaf
	((td->core_profile_trace_data.epoch->csdata).tree_root, prefix);
#endif
    }
    else {
      prefix = hpcrun_cct_insert_path_return_leaf
	((td->core_profile_trace_data.epoch->csdata).unresolved_root, prefix);
      ompt_region_map_refcnt_update(partial_region_id, 1L);
      TMSG(DEFER_CTXT, "omp_resolve: get partial resolution to 0x%lx\n", partial_region_id);
    }
    // adjust the callsite of the prefix in side threads to make sure they are the same as
    // in the master thread. With this operation, all sides threads and the master thread
    // will have the unified view for parallel regions (this only works for GOMP)
    if (td->team_master) {
      hpcrun_cct_merge(prefix, cct, merge_metrics, NULL);
    }
    else {
      hpcrun_cct_merge(prefix, cct, merge_metrics, NULL);
      // must delete it when not used considering the performance
      TMSG(DEFER_CTXT, "omp_resolve: resolve region 0x%lx", my_region_id);
      ompt_region_map_refcnt_update(my_region_id, -1L);
    }
  }
}


static void
omp_resolve_and_free(cct_node_t* cct, cct_op_arg_t a, size_t l)
{
  omp_resolve(cct, a, l);
}


int 
need_defer_cntxt()
{
  if (ENABLED(OMPT_LOCAL_VIEW)) return 0;

  // master thread does not need to defer the context
  if ((hpcrun_ompt_get_parallel_id(0) > 0) && !TD_GET(master)) {
    thread_data_t *td = hpcrun_get_thread_data();
    td->defer_flag = 1;
    return 1;
  }
  return 0;
}


uint64_t 
is_partial_resolve(cct_node_t *prefix)
{
  //go up the path to check whether there is a node with UNRESOLVED tag
  cct_node_t *node = prefix;
  while (node) {
    cct_addr_t *addr = hpcrun_cct_addr(node);
    if (IS_UNRESOLVED_ROOT(addr))
      return (uint64_t)(addr->ip_norm.lm_ip);
    node = hpcrun_cct_parent(node);
  }
  return 0;
}


//-----------------------------------------------------------------------------
// Function: resolve_cntxt
// 
// Purpose: 
//   resolve contexts of parallel regions.
//
// Description:
// (1) Compute the outer-most region id; only the outer-most region needs to be 
//     resolved
// (2) If the thread has a current region id that is different from its previous 
//     one; and the previous region id is non-zero, resolve the previous region.
//     The previous region id is recorded in td->region_id.
// (3) If the thread has a current region id that is different from its previous
//     one; and the current region id is non-zero, add a slot into the 
//     unresolved tree indexed by the current region_id
// (4) Update td->region_id to be the current region id

void resolve_cntxt()
{
  cct_node_t* tbd_cct = (hpcrun_get_thread_epoch()->csdata).unresolved_root;
  thread_data_t *td = hpcrun_get_thread_data();

  //---------------------------------------------------------------------------
  // step 1: 
  // 
  // a pure worker thread's outermost region is the same as its innermost region.
  // 
  // a sub-master thread's outermost region was memoized when the thread became
  // a sub-master. the identity of the sub-master's outermost region in this case is 
  // available in td->outer_region_id.
  //
  // the post condition for the following code is that outer_region_id contains 
  // the outermost region id in the current thread.
  //---------------------------------------------------------------------------

  uint64_t innermost_region_id = hpcrun_ompt_get_parallel_id(0); 
  uint64_t outer_region_id = 0;

  if (td->outer_region_id > 0) {
    uint64_t enclosing_region_id = hpcrun_ompt_get_parallel_id(1); 
    if (enclosing_region_id == 0) {
      // we are currently in a single level parallel region.
      // forget the previously memoized outermost parallel region. 
      td->outer_region_id = 0;
    } else {
      outer_region_id = td->outer_region_id; // outer region for submaster 
    }
  }

  // if outer_region_id has not already been set, it defaults to the innermost 
  // region.
  if (outer_region_id == 0) {
    outer_region_id = innermost_region_id;
  }

  TMSG(DEFER_CTXT, "resolve_cntxt: outermost region id is 0x%lx, "
       "innermost region id is 0x%lx", outer_region_id, innermost_region_id); 

  //---------------------------------------------------------------------------
  // step 2: 
  //
  // if we changed parallel regions, try to resolve contexts for past regions.
  //---------------------------------------------------------------------------
  if (td->region_id != 0) { 
    // we were in a parallel region when the last sample was received
    if (td->region_id != outer_region_id) {
      // the region we are in now (if any) differs from the region where
      // the last sample was received.
      TMSG(DEFER_CTXT, "exited region 0x%lx; attempting to resolve contexts", td->region_id);
      hpcrun_cct_walkset(tbd_cct, omp_resolve_and_free, td);
    }
  }

  //
  // part 3: insert the current region into unresolved tree (tbd tree)
  //
  // update the use count when come into a new omp region
  if ((td->region_id != outer_region_id) && (outer_region_id != 0)) {
    // end_team_fn occurs at master thread, side threads may still 
    // in a barrier waiting for work. Now the master thread may delete
    // the record if no samples taken in it. But side threads may take 
    // samples in the waiting region (which has the same region id with
    // the end_team_fn) after the region record is deleted.
    // solution: consider such sample not in openmp region (need no
    // defer cntxt) 
    if (ompt_region_map_refcnt_update(outer_region_id, 1L))
      hpcrun_cct_insert_addr(tbd_cct, &(ADDR2(UNRESOLVED, outer_region_id)));
    else
      outer_region_id = 0;
  }

#ifdef DEBUG_DEFER
  int initial_td_region = td->region_id;
#endif

  //
  // part 4: update the td->region_id, indicating the region where this thread 
  // most-recently takes a sample.
  //
  // td->region_id represents the out-most parallel region id
  td->region_id = outer_region_id;

#ifdef DEBUG_DEFER
  // debugging code
  if (innermost_region_id) {
    ompt_region_map_entry_t *record = ompt_region_map_lookup(innermost_region_id);
    if (!record || (ompt_region_map_entry_refcnt_get(record) == 0)) {
      EMSG("no record found innermost_region_id=0x%lx initial_td_region_id=0x%lx td->region_id=0x%lx ", 
	   innermost_region_id, initial_td_region, td->region_id);
    }
  }
#endif
}


void 
resolve_cntxt_fini(thread_data_t *td)
{
  hpcrun_cct_walkset(td->core_profile_trace_data.epoch->csdata.unresolved_root, 
                     omp_resolve_and_free, td);
}


cct_node_t *
hpcrun_region_lookup(uint64_t id)
{
  cct_node_t *result = NULL;

  ompt_region_map_entry_t *record = ompt_region_map_lookup(id);
  if (record) {
    result = ompt_region_map_entry_callpath_get(record);
  }

  return result;
}

