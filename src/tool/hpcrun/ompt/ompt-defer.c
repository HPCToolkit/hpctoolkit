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

#include "ompt-thread.h"

#include "ompt-interface.h"
#include "ompt-region-map.h"




#include "../../../lib/prof-lean/stdatomic.h"
#include "../thread_data.h"
#include "../cct/cct_bundle.h"

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>
#include <hpcrun/unresolved.h>







#include "ompt.h"



#include "../memory/hpcrun-malloc.h"
#include "../cct/cct.h"
#include "../thread_data.h"
#include "../cct/cct_bundle.h"
#include "../unresolved.h"
#include "../../../lib/prof-lean/hpcrun-fmt.h"
#include "../utilities/ip-normalized.h"
#include "../messages/debug-flag.h"


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

//  ompt_data_t *parallel_data = NULL;
//  int team_size = 0;
//  hpcrun_ompt_get_parallel_info(0, &parallel_data, &team_size);
//  uint64_t my_region_id = parallel_data->value;
//
//  printf("Resolving: %lx! \n", my_region_id);

  TMSG(DEFER_CTXT, "omp_resolve: try to resolve region 0x%lx", my_region_id);
  uint64_t partial_region_id = 0;
  prefix = hpcrun_region_lookup(my_region_id);
  //printf("Prefix = %p\n", prefix);
  if (prefix) {
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
      //FIXME: Get region_data_t and update refcnt
//      ompt_region_map_refcnt_update(partial_region_id, 1L);
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
      //ompt_region_map_refcnt_update(my_region_id, -1L);
      //ompt_region_map_refcnt_update(my_region_id, -1L);
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
  if ((hpcrun_ompt_get_parallel_info_id(0) > 0) && !TD_GET(master)) {
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
  return;
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

  uint64_t innermost_region_id = hpcrun_ompt_get_parallel_info_id(0);
  uint64_t outer_region_id = 0;

  if (td->outer_region_id > 0) {
    uint64_t enclosing_region_id = hpcrun_ompt_get_parallel_info_id(1);
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


    // FIXME: Accomodate to region_data_t
    // FIXME: Focus on this
    // (ADDR2(UNRESOLVED, get from region_data)
    // watch for monitoring variables

//    if (ompt_region_map_refcnt_update(outer_region_id, 1L))
//      hpcrun_cct_insert_addr(tbd_cct, &(ADDR2(UNRESOLVED, outer_region_id)));
//    else
//      outer_region_id = 0;


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
  //printf("Resolving for thread... region = %d\n", td->region_id);
  //printf("Root children = %p\n", td->core_profile_trace_data.epoch->csdata.unresolved_root->children);
  hpcrun_cct_walkset(td->core_profile_trace_data.epoch->csdata.unresolved_root, 
                     omp_resolve_and_free, td);
}


cct_node_t *
hpcrun_region_lookup(uint64_t id)
{
  cct_node_t *result = NULL;

  // FIXME: Find another way to get infor about parallel region

//  ompt_region_map_entry_t *record = ompt_region_map_lookup(id);
//  if (record) {
//    result = ompt_region_map_entry_callpath_get(record);
//  }

  return result;
}

// added by vi3

void add_to_list(ompt_region_data_t* region_data){
  ompt_trl_el_t* new_region = hpcrun_ompt_trl_el_alloc();
  new_region->region_data = region_data;
  OMPT_BASE_T_GET_NEXT(new_region) = OMPT_BASE_T_STAR(registered_regions);
  if(registered_regions)
    registered_regions->prev = new_region;
  registered_regions = new_region;
  new_region->prev = NULL;
}

void remove_from_list(ompt_region_data_t* region_data){
  ompt_trl_el_t* current = registered_regions;
  while(current){
    if(current->region_data == region_data){
      if(current->prev){
        OMPT_BASE_T_GET_NEXT(current->prev) = OMPT_BASE_T_GET_NEXT(current);
      } else{
        registered_regions = (ompt_trl_el_t*)OMPT_BASE_T_GET_NEXT(current);
      }

      if(OMPT_BASE_T_GET_NEXT(current)){
        ((ompt_trl_el_t*)OMPT_BASE_T_GET_NEXT(current))->prev = current->prev;
      }

      break;
    }
    current = (ompt_trl_el_t*)OMPT_BASE_T_GET_NEXT(current);
  }
  hpcrun_ompt_trl_el_free(current);
}

void
register_to_region(ompt_region_data_t* region_data){
  // push to stack
  push_region_stack(region_data->region_id);
  // create notification and enqueu to region's queue
  ompt_notification_t* notification = hpcrun_ompt_notification_alloc();
  notification->region_data = region_data;
  notification->threads_queue = &threads_queue;
  OMPT_BASE_T_GET_NEXT(notification) = NULL;
  // register thread to region's wait free queue
  wfq_enqueue(OMPT_BASE_T_STAR(notification), &region_data->queue);
  // store region data to thread's queue
  add_to_list(region_data);
}

void
register_to_all_existing_regions(ompt_region_data_t* current_region, ompt_region_data_t* parent_region){
  clear_region_stack();
  // if only one parallel region, then is enough just to register thread to it
  if(!parent_region){
    register_to_region(current_region);
    return;
  }

  // temporary array that contains all regions from nested regions subtree
  // where thread belongs to
  ompt_region_data_t* temp_regions[MAX_NESTING_LEVELS];
  // add current and parent region
  temp_regions[0] = current_region;
  temp_regions[1] = parent_region;

  // start from grandparent
  int ancestor_level = 2;
  ompt_region_data_t* current_ancestor = hpcrun_ompt_get_region_data(ancestor_level);
  // while we have ancestor
  while(current_ancestor){
    // add ancestor region id to temporary array
    temp_regions[ancestor_level] = current_ancestor;
    // go one level upper in regions subtree
    current_ancestor = hpcrun_ompt_get_region_data(++ancestor_level);
  }

  // on the ancestor_level we don't have any regions,
  // so we are going one step deeper where is the top most region's of subtree
  ancestor_level--;

  // reversed temporary array is proper content of  stack

  // go from the end of array, and push region's id to stack
  while (ancestor_level){
    register_to_region(temp_regions[ancestor_level--]);
  }
}

void
register_to_all_regions(){
  // FIXME vi3: maybe we should add thread local variable to store current region data
  ompt_region_data_t* current_region = hpcrun_ompt_get_current_region_data();
  // no parallel region
  if(!current_region)
    return;

  // stack is empty, register thread to all existing regions
  if(is_empty_region_stack()){
    // we wil provide parent region data by calling helper function inlined
    // with function for registration
    register_to_all_existing_regions(current_region, hpcrun_ompt_get_parent_region_data());
    return;
  }

  // is thread already register to region that is on the top of the stack
  // if that is the case, nothing has to be done
  uint64_t top = top_region_stack();
  if(top == current_region->region_id){
    return;
  }

  // Thread is not register to current region.

  // If there is not parent region and stack is not clear, then regions
  // from the stack should be removed. After that, all we need to do is to
  // register thread to the current region, so that stack will only contain it.
  ompt_region_data_t* parent_region = hpcrun_ompt_get_parent_region_data();
  if (!parent_region){
    register_to_all_existing_regions(current_region, NULL);
    return;
  }


  // if parent region is at the top of the stack
  // all we have to do is register thread to current region
  if(top == parent_region->region_id){
    register_to_region(current_region);
    return;
  }


  // Thread is not registered to the current region and parent region is
  // not on the stack, which means that thread change nested regions subtree (change the team).
  // All regions from stack should be removed and regions from current subtree should be added.
  register_to_all_existing_regions(current_region, parent_region);

}



void try_resolve_context(){
  // We should use wfq_dequeue_private, because removing and adding are happenig
  // at the same time. Different than resolving_all_remaining_context.

  ompt_notification_t *old_head = NULL;
  while ((old_head = (ompt_notification_t*) wfq_dequeue_private(&threads_queue,
                                                                OMPT_BASE_T_STAR_STAR(private_threads_queue))) ){

    ompt_region_data_t* region_data = old_head->region_data;
    // printf("\n\n\nResolving one region from idle!\n\n");
    resolve_one_region_context(region_data);

    hpcrun_ompt_notification_free(old_head);

    // any thread should be notify
    ompt_notification_t* to_notify = (ompt_notification_t*) wfq_dequeue_public(&region_data->queue);
    if(to_notify){
      wfq_enqueue(OMPT_BASE_T_STAR(to_notify), to_notify->threads_queue);
    }else{
      // notify creator of region that region_data can be put in region's freelist
      hpcrun_ompt_region_free(region_data);
    }

    remove_from_list(region_data);

  }
}


void resolve_one_region_context(ompt_region_data_t* region_data){

  cct_node_t* call_path = region_data->call_path;

  cct_node_t* root = region_root(call_path);
  cct_node_t* prefix =  hpcrun_cct_insert_path_return_leaf(root, call_path);
  cct_node_t* unr_root = hpcrun_get_thread_data()->core_profile_trace_data.epoch->csdata.unresolved_root;
  cct_node_t* to_move = hpcrun_cct_find_addr(unr_root, &(ADDR2(UNRESOLVED, region_data->region_id)));

  // FIXME: possible point of losing metrics (just skip this region, cause it is resolved already)
  if(!to_move){
    return;
  }

  hpcrun_cct_merge(prefix, to_move, merge_metrics, NULL);

  hpcrun_cct_delete_self(to_move);
}


void resolving_all_remaining_context(){

  ompt_trl_el_t* current = registered_regions;
  ompt_trl_el_t* previous;

  while(current){
    // FIXME: be sure that current->region_data is not NULL
    resolve_one_region_context(current->region_data);
    previous = current;
    current = (ompt_trl_el_t*)OMPT_BASE_T_GET_NEXT(current);
    // FIXME: call for now this function
    remove_from_list(previous->region_data);
  }

  // FIXME: find all memory leaks

}


// DONE: deallocation when there is no one register to region





// DONE: use stack for checking, and pop anything that is not parent of current regions (example below)
/*
 *            1
 *          2    3
 *        4  5  6  7
 * */




/*
 * void insert_chain(item_type *first, item_type *last) {
   // this loop is finite; even if the CAS fails because the worker is popping elements out of the queue,
   // it can only fail a bounded number of times because
   //   (1) the queue is of finite length at any time, and
   //   (2) this thread is the only one inserting into the queue
   do {
        item_type *old_head = _head->load();
        last->_next.store(old_head);
   } while (! _head->compare_and_swap(old_head, first));
  };

  item_type *next() {
     LFQ_DEBUG(assert(validate == this));
     item_type *succ = _next.load();
     return succ;
};
 *
 *
 * */



// FIXME: \(ompt_\w+_t\s*\*\)




