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
#include <unwind/common/backtrace_info.h>


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

cct_node_t*
add_pseudo_cct (ompt_region_data_t* region_data) {
  // should add cct inside the tree
  cct_node_t* new;
  if(top_index == 0) {
    // this is the first parallel region add pseudo cct
    // which corresponds to the region as a child of thread root
    new = hpcrun_cct_insert_addr((hpcrun_get_thread_epoch()->csdata).thread_root,
                                 &(ADDR2(UNRESOLVED, region_data->region_id)));
//    printf("This is my parent: %p, this is unresolved root: %p\n",
//           hpcrun_cct_parent(new), (hpcrun_get_thread_epoch()->csdata).thread_root);
  } else {
    // add cct as a child of a previous pseudo cct
    new = hpcrun_cct_insert_addr(region_stack[top_index - 1]->unresolved_cct,
                                 &(ADDR2(UNRESOLVED, region_data->region_id)));
  }

  return new;
}

void
register_to_region(ompt_notification_t* notification){
  ompt_region_data_t* region_data = notification->region_data;
  // create notification and enqueu to region's queue
  OMPT_BASE_T_GET_NEXT(notification) = NULL;
  // register thread to region's wait free queue
  wfq_enqueue(OMPT_BASE_T_STAR(notification), &region_data->queue);
  // store region data to thread's queue
  //add_to_list(region_data);
  // increment the number of unresolved regions
  unresolved_cnt++;
  // add unresolved cct to notification
  notification->unresolved_cct = add_pseudo_cct(region_data);

}

ompt_notification_t*
add_notification_to_stack(ompt_region_data_t* region_data) {
    ompt_notification_t* notification = hpcrun_ompt_notification_alloc();
    notification->region_data = region_data;
    notification->threads_queue = &threads_queue;
    // push to stack
    push_region_stack(notification);
    return notification;
}

void
register_if_not_master(ompt_notification_t *notification)
{
  if(notification && not_master_region == notification->region_data) {
    register_to_region(notification);
    // should memoize the cct for not_master_region
    cct_not_master_region = notification->unresolved_cct;
  }
}

// previous solution
//if(not_master_region == current_region){
//  register_to_region(notification);
//  // should memoize the cct for not_master_region
//  cct_not_master_region = notification->unresolved_cct;
//}


void
register_to_all_existing_regions(ompt_region_data_t* current_region, ompt_region_data_t* parent_region){
  clear_region_stack();
  // if only one parallel region, then is enough just to register thread to it
  if(!parent_region){
    // add notification to the stack
    ompt_notification_t* notification = add_notification_to_stack(current_region);
    // if this region is not_master_region, then thread should register itself
    register_if_not_master(notification);
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

  // boolean which tells if not_master_region has been found during iteration
  // it indicates that all remaining regions in temp_regions array are nested inside
  // not_master_regions, which means that thread is the master of all of them.
  int found_not_master = 0;
  ompt_region_data_t* temp;
  ompt_notification_t* notification;
  // go from the end of array, and push region's id to stack
  while (ancestor_level >= 0){
    // add notification to stack
    temp = temp_regions[ancestor_level--];
    notification = add_notification_to_stack(temp);
    // if we haven't found memoized not master region
    if(!found_not_master) {
        // that means that thread needs to register itself in region's queue
        register_to_region(notification);
        // check if we found not_master_region
        if(temp == not_master_region){
          found_not_master = 1;
          // should memoize the cct for not_master_region
          cct_not_master_region = notification->unresolved_cct;
        }
    }
  }

  if (! found_not_master) {
      printf("----------------Something is really bad here, THREAD: %p\n", &threads_queue);
  }

}

//
int
is_already_registered(ompt_region_data_t* region_data){
 // FIXME: I think that is ok just to check the top of the stack
 // only if we remove the top of the region inside parallel end callback
    int i;
    for(i = top_index; i >= 0; i--) {
      if(region_stack[i]->region_data->region_id == region_data->region_id) {
        // FIXME: should we change the top of the stack
        if (top_index != i) {
          // FIXME vi3: should this ever happened
          // this happened
          //printf("Consider to keep this way\n");
        }
        top_index = i;
        return 1;
      }
    }
    return 0;
}

//if(not_master_region == current_region) {
//  register_to_region(notification);
//  // should memoize the cct for not_master_region
//  cct_not_master_region = notification->unresolved_cct;
//}


void
register_to_all_regions(){
  // FIXME vi3: maybe we should add thread local variable to store current region data
  ompt_region_data_t* current_region = hpcrun_ompt_get_current_region_data();
  // no parallel region
  if(!current_region) {
    // empty stack
    clear_region_stack();
    return;
  }
  // stack is empty, register thread to all existing regions
  if(is_empty_region_stack()){
    // we wil provide parent region data by calling helper function inlined
    // with function for registration
    register_to_all_existing_regions(current_region, hpcrun_ompt_get_parent_region_data());
    return;
  }

  // is thread already register to region that is on the top of the stack
  // if that is the case, nothing has to be done
  if(is_already_registered(current_region))
      return;

  // Thread is not register to current region.

  // If there is not parent region and stack is not clear, then regions
  // from the stack should be removed. After that, all we need to do is to
  // register thread to the current region, so that stack will only contain it.
  ompt_region_data_t* parent_region = hpcrun_ompt_get_parent_region_data();
  if (!parent_region){
    register_to_all_existing_regions(current_region, NULL);
    return;
  }

  ompt_notification_t* top = top_region_stack();
  // if parent region is at the top of the stack
  // all we have to do is register thread to current region
  if(top && top->region_data->region_id == parent_region->region_id){
    // add notification to the stack
    ompt_notification_t* notification = add_notification_to_stack(current_region);
    // if the thread is in the not_master_region, it should register for region's callpath
    // FIXME: check if this condition is right
    register_if_not_master(notification);
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

  // FIXME: move this to the end
  unresolved_cnt--;

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


  return_label: {
    hpcrun_cct_delete_self(to_move);
  };

}

// return one if successful
int
try_resolve_one_region_context()
{
  ompt_notification_t *old_head = NULL;
  old_head = (ompt_notification_t*) wfq_dequeue_private(&threads_queue, OMPT_BASE_T_STAR_STAR(private_threads_queue));
  if(!old_head)
    return 0;
  // region to resolve
  ompt_region_data_t *region_data = old_head->region_data;

  // ================================== resolving part

  cct_node_t *unresolved_cct = old_head->unresolved_cct;
  cct_node_t *parent_unresolved_cct = hpcrun_cct_parent(unresolved_cct);

  if(region_data->call_path == NULL) {
    printf("Ne bi smeo ti ovo da uradis nikako!\n");
  }

  // prefix should be put between unresolved_cct and parent_unresolved_cct
  cct_node_t *prefix = hpcrun_cct_insert_path_return_leaf(parent_unresolved_cct, region_data->call_path);

//  printf("THREAD: %p, RESOLVING: %lx, UNR_CCT: %lx, IS_PARENT_THREAD_ROOT: %d, PARENT_CCT: %lx, PREFIX: %lx\n",
//         &threads_queue,
//         region_data->region_id,
//         hpcrun_cct_addr(unresolved_cct)->ip_norm.lm_ip,
//         hpcrun_get_thread_epoch()->csdata.thread_root == parent_unresolved_cct,
//         hpcrun_cct_addr(parent_unresolved_cct)->ip_norm.lm_ip,
//         hpcrun_cct_addr(prefix)->ip_norm.lm_ip
//         );

  // prefix node should change the unresolved_cct
  hpcrun_cct_merge(prefix, unresolved_cct, merge_metrics, NULL);
  // delete unresolved_cct from parent
  hpcrun_cct_delete_self(unresolved_cct);

  // ==================================


    // FIXME: add unresolved_cct to freelist if needed
  // free notification
  hpcrun_ompt_notification_free(old_head);


  // any thread should be notify
  ompt_notification_t* to_notify = (ompt_notification_t*) wfq_dequeue_public(&region_data->queue);
  if(to_notify){
    wfq_enqueue(OMPT_BASE_T_STAR(to_notify), to_notify->threads_queue);
  }else{
    // notify creator of region that region_data can be put in region's freelist
    hpcrun_ompt_region_free(region_data);
  }

  unresolved_cnt--;

//  printf("I have successfully resolved. This is my parent: %p\n", parent_unresolved_cct);
  return 1;
}

void resolving_all_remaining_context(){
  // resolve all remaining regions
  while(unresolved_cnt) {
    try_resolve_one_region_context();
  }



  // old implementation
//  return;
//
//  ompt_trl_el_t* current = registered_regions;
//  ompt_trl_el_t* previous;
//
//  while(current){
//    // FIXME: be sure that current->region_data is not NULL
//    resolve_one_region_context(current->region_data);
//    previous = current;
//    current = (ompt_trl_el_t*)OMPT_BASE_T_GET_NEXT(current);
//    // FIXME: call for now this function
//    remove_from_list(previous->region_data);
//  }
//
//  // FIXME: find all memory leaks

}

cct_node_t*
top_cct(cct_node_t* current_cct) {
  if (!current_cct)
    return NULL;

  cct_node_t *temp = current_cct;
  // FIXME: optimize this
  while(hpcrun_cct_parent(temp)) {
      temp = hpcrun_cct_parent(temp);
  }
  return temp;
}


#define UINT64_T(value) (uint64_t)value

// first_frame_above
frame_t*
first_frame_above(frame_t *start, frame_t *end, uint64_t frame_address, int *index)
{
  frame_t *it;
  for(it = start; it <= end; it++, (*index)++) {
    // FIXME: exit frame of current should be the same as reenter of previous frane
    if(UINT64_T(it->cursor.sp) >= frame_address){
      return it;
    }
  }
  return NULL;
}

// first_frame_below
frame_t*
first_frame_below(frame_t *start, frame_t *end, uint64_t frame_address, int *index)
{
  frame_t *it = first_frame_above(start, end, frame_address, index);
  if(!it) {
    return NULL;
  }

  // we are now one frame above, should go one frame below
  it--;
  (*index)--;

  if(frame_address > UINT64_T(it->cursor.sp)) {
    //printf("***********first_frame_below********Inside user code\n");
  } else if(frame_address == UINT64_T(it)) {
    printf("***********first_frame_below********The same address\n");
  } else {
    printf("***********first_frame_below********Something is bad\n");
  }

  return it;
}



cct_node_t*
get_cct_from_prefix(cct_node_t* cct, int index)
{
  if(!cct)
    return NULL;

  // FIXME: this is just a temporary solution
  cct_node_t* current = cct;
  int current_index = 0;
  while(current) {
    if(current_index == index) {
      return current;
    }
    current = hpcrun_cct_parent(current);
    current_index++;
  }
  return NULL;
}

cct_node_t*
copy_prefix(cct_node_t* top, cct_node_t* bottom)
{
  // FIXME: vi3 do we need to copy? find the best way to copy callpath
  // previous implementation
  // return bottom;

  // direct manipulation with cct nodes, which is probably not good way to solve this

  if(!bottom || !top){
    return NULL;
  }

  cct_node_t *prefix_bottom = hpcrun_cct_copy_just_addr(bottom);
  // it is possible that just one node is call path between regions
  if(top == bottom) {
    return prefix_bottom;
  }

  cct_node_t *it = hpcrun_cct_parent(bottom);
  cct_node_t *child = NULL;
  cct_node_t *parent = prefix_bottom;

  while (it) {
    child = parent;
    parent = hpcrun_cct_copy_just_addr(it);
    hpcrun_cct_set_parent(child, parent);
    hpcrun_cct_set_children(parent, child);
    // find the top
    if(it == top) {
      return prefix_bottom;
    }
    it = hpcrun_cct_parent(it);
  }

  return NULL;

}

// Check whether we found the outermost region in which current thread is the master
// returns -1 when there is no regions
// returns 0 if the region is not the outermost region of which current thread is master
// returns 1 when the region is the outermost region of which current thread is master
int
is_outermost_region_thread_is_master(int stack_index)
{
  // no regions
  if(is_empty_region_stack())
    return -1;
  // thread is initial master, and we found the region which is on the top of the stack
  if(TD_GET(master) && stack_index == 0)
    return 1;

  // thread is not the master, and region that is upper on the stack is not_master_region
  if(!TD_GET(master) && stack_index && region_stack[stack_index - 1]->region_data == not_master_region){
    return 1;
  }

  return 0;
}

void
print_prefix_info(char *message, cct_node_t *prefix, ompt_region_data_t *region_data, int stack_index, backtrace_info_t *bt, cct_node_t *cct)
{
    // prefix length
    int len_prefix = 0;
    cct_node_t *tmp_top = NULL;
    cct_node_t *tmp_bottom = NULL;
    cct_node_t *tmp = NULL;
    tmp_bottom = prefix;
    tmp = prefix;
    while (tmp) {
      len_prefix++;
      tmp_top = tmp;
      tmp = hpcrun_cct_parent(tmp);
    }
    // number of frames
    int len_bt = 0;
    frame_t *bt_inner = bt->begin;
    frame_t *bt_outer = bt->last;
    // frame iterator
    frame_t *it = bt_inner;
    while(it <= bt_outer) {
      len_bt++;
      it++;
    }

    // number of cct nodes
    int len_cct = 0;
    cct_node_t *current = cct;
    while (current) {
      len_cct++;
      current = hpcrun_cct_parent(current);
    }

//    printf("%s REGION_IND: %lx STACK_INDEX: %d MASTER: %d TOP CCT: %p BOTOOM CCT: %p PEF_LENGTH: %d FRAME_NUMBER: %d CCT_NUMBER: %d\n",
//           message, region_data->region_id, stack_index, TD_GET(master), tmp_top, tmp_bottom, len_prefix, len_bt, len_cct);
}

// check if the thread cannot resolved region because of reasons mentioned below
int
this_thread_cannot_resolve_region(ompt_notification_t *current_notification)
{
  // if current notification is null, or the thread is not the master
  // or the path is already resolved
  return !current_notification
         || current_notification->region_data == not_master_region
         || current_notification->region_data->call_path != NULL;
}

void
provide_callpath_for_regions_if_needed(backtrace_info_t* bt, cct_node_t* cct)
{
   // return;
  if(bt->partial_unwind) {
    printf("---------------------Ovde nesto vode ne pije\n");
  }

//  if(!TD_GET(master))
//    return;
  // not regions on the stack, should mean that we are not inside parallel region
  if(is_empty_region_stack())
    return;

  // if thread is not the master of the region, or the region is resolved, then just return
  ompt_notification_t* current_notification = top_region_stack();
  if(this_thread_cannot_resolve_region(current_notification)){
    // this happened
    //printf("I am not responsible for this region\n");
    return;
  }

  int index = 0;
  ompt_frame_t* current_frame = hpcrun_ompt_get_task_frame(0);
  if(!current_frame){
    return;
  }
  frame_t *bt_inner = bt->begin;
  frame_t *bt_outer = bt->last;
  // frame iterator
  frame_t *it = bt_inner;

    //printf("Inner has smaller address: %d\n", UINT64_T(bt_inner->cursor.sp) < UINT64_T(bt_outer->cursor.sp));


  if(UINT64_T(current_frame->reenter_runtime_frame) == 0
     && UINT64_T(current_frame->exit_runtime_frame) != 0){
    // thread take a sample inside user code of the parallel region
    // this region is the the innermost

    // this has happened
    //printf("First time in user code of the parallel region\n");

    it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
    if(it == NULL){

      return;
    }
    // this happened
    //printf("One above exit\n");

  } else if(UINT64_T(current_frame->reenter_runtime_frame) <= UINT64_T(bt_inner->cursor.sp)
            && UINT64_T(bt_inner->cursor.sp) <= UINT64_T(current_frame->exit_runtime_frame)) { // FIXME should put =
    // thread take a simple in the region which is not the innermost
    // all innermost regions have been finished

    // this happened
    //printf("Second time in user code of the parallel region\n");

    it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
    if(it == NULL){
      // this happened once for the thread da it is not TD_GET(master)

      return;
    }
    // this happened
    //printf("One above exit...\n");

  } else if ( UINT64_T(bt_inner->cursor.sp) < UINT64_T(current_frame->reenter_runtime_frame)) {
    // take a sample inside the runtime
    // this happened
    //printf("Sample has been taken inside runtime frame\n");

      return;

  } else if (UINT64_T(current_frame->reenter_runtime_frame) == 0
             && UINT64_T(current_frame->exit_runtime_frame) == 0) {
    // FIXME: check what this means
    // this happened
    //printf("Both reenter and exit are zero\n");

      // this happened a couple of time for not TD_GET(master) threads

    return;

  } else if(UINT64_T(current_frame->exit_runtime_frame) == 0
            && UINT64_T(bt_inner->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)) {
    //ompt_frame_t* parent = hpcrun_ompt_get_task_frame(1);
    //printf("I did not cover. BT_INNNER: %lu\tEXIT: %lu\tREENTER: %lu\tREGIONS: %d\tMASTER: %d\tPARENT FRAME: %p\n",
    //       UINT64_T(bt_inner->cursor.sp), UINT64_T(current_frame->exit_runtime_frame),
    //       UINT64_T(current_frame->reenter_runtime_frame), top_index+1, TD_GET(master), parent);

    // FIXME: the master thread is probably inside implicit task
    // this happened
    //printf("Master thread is inside implicit task\n");


    return;

  } else {
    printf("Something that has not been cover\n");

    return;
  }


  int frame_level = 0;
  int stack_index = top_index;

  cct_node_t* bottom_prefix = NULL;
  cct_node_t* top_prefix = NULL;
  cct_node_t* prefix = NULL;

    // at the beginning of the loop we should be one frame above the end_frame
  // which means that we are inside user code of the parent region or the initial implicit task
  // if the current thread is the initial master
  // while loop
  for(;;) {
    frame_level++;
    current_frame = hpcrun_ompt_get_task_frame(frame_level);
    if(!current_frame) {
      printf("************************This makes problem\n");
      return;
    }

    // we should be inside the user code of the parent parallel region
    // but let's check that
    if(UINT64_T(it->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)){
      // this happened

      // printf("Inside user code\n");
    } else {
      // do nothing for now
      printf("*********************************Inside runtime\n");
      return;
      // TODO: if this would happen, then go up until
      // UINT64_T(it->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)
    }

    bottom_prefix = get_cct_from_prefix(cct, index);
    if(!bottom_prefix){
      printf("Should this ever happened\n");
      return;
    }

    // we found the end of the parent, next thing is to find the beginning of the parent
    // if this is the last region on the stack, then all remaining cct should be in the prefix
    if(TD_GET(master) && stack_index == 0) {
      top_prefix = top_cct(bottom_prefix);
      // FIXME: should we copy prefix, or is enough just to set region callpath
      prefix = copy_prefix(top_prefix, bottom_prefix);
      if (!prefix) {
        printf("Should this ever happen?\n");
        return;
      }
      current_notification->region_data->call_path = prefix;

      print_prefix_info("", prefix, current_notification->region_data, stack_index, bt, cct);

      return;
    }

    // we are inside user code of outside region
    // should find one frame below exit_runtime_frame
    it = first_frame_below(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);

    top_prefix = get_cct_from_prefix(cct, TD_GET(master) ? index + 1 : index);
    prefix = copy_prefix(top_prefix, bottom_prefix);
    if(!prefix) {
        printf("What to do now?\n");
        return;
    }
    current_notification->region_data->call_path = prefix;
    print_prefix_info("", prefix, current_notification->region_data, stack_index, bt, cct);

    // next thing to do is to get parent region notification from stack
    stack_index--;
    if(stack_index < 0) {
      printf("***********Something bad is hapenning\n");
      return;
    }
    current_notification = region_stack[stack_index];
    if(this_thread_cannot_resolve_region(current_notification)) {
      // this happened
      //printf("THREAD FINISHED EVERYTHING\n");
      return;
    }
    // go to parent region frame and do everything again
    it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);


  }


    // FIXME: vi3 for now, consider only cases when sample is taken inside parallel region's user code
// // we are now one frame above the current_frame->exit_runtime frame

//  // we need to go one level upper in runtime frame
//  current_frame = hpcrun_ompt_get_task_frame(1);
//
//  if(!current_frame) {
//    printf("************************This makes problem\n");
//    return;
//  }


//  // we should be inside the user code of the parent parallel region
//  // but let's check that
//  if(UINT64_T(it->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)){
//    // this happened
//
//    // printf("Inside user code\n");
//  } else {
//    // do nothing for now
//    printf("*********************************Inside runtime\n");
//    return;
//    // TODO: if this would happen, then go up until
//    // UINT64_T(it->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)
//  }



//  // FIXME: ovaj deo koda kurcu ne valja
//  // we should be at the bottom of the parent region
//  cct_node_t* bottom_prefix = NULL;
//  cct_node_t* top_prefix = NULL;
//  cct_node_t* prefix = NULL;

//  return;

//  bottom_prefix = get_cct_from_prefix(cct, index);
//  if(!bottom_prefix){
//    printf("Should this ever happened\n");
//    return;
//  }

//  // we found the end of the parent, next thing is to find the beginning of the parent
//  // if this is the only region of the stack, then all remaining cct should be in the prefix
//  if(TD_GET(master) && top_index == 0){
//    top_prefix = top_cct(bottom_prefix);
//    // FIXME: should we copy prefix, or is enough just to set region callpath
//    prefix = copy_prefix(top_prefix, bottom_prefix);
//    if(!prefix){
//      printf("Should this ever happen?\n");
//      return;
//    }
//    current_notification->region_data->call_path = prefix;
//
//    print_prefix_info("OUTERMOST ", prefix);
//    return;
//  }

  // check what to do when you are not master, and have one enclosing region


//  // we are inside user code of outside region
//  // should find one frame below exit_runtime_frame
//  it = first_frame_below(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);

//  if(!it) {
//    // this happend for not TD_GET(master) thread
//    printf("outer: %lu, exit: %lu\n", UINT64_T(bt_outer->cursor.sp), UINT64_T(current_frame->exit_runtime_frame));
//    return;
//  }

//  top_prefix = get_cct_from_prefix(cct, TD_GET(master) ? index + 1 : index);
//  prefix = copy_prefix(top_prefix, bottom_prefix);
//  if(!prefix) {
//    printf("What to do now?\n");
//    return;
//  }
//  current_notification->region_data->call_path = prefix;
//  print_prefix_info("INNERRMOST", prefix);

//  printf("INNERMOST: EQUALS: %d, %d\n",
//         UINT64_T(hpcrun_ompt_get_task_frame(0)->exit_runtime_frame) ==
//         UINT64_T(hpcrun_ompt_get_task_frame(1)->reenter_runtime_frame),
//         UINT64_T(hpcrun_ompt_get_task_frame(1)->exit_runtime_frame) ==
//         UINT64_T(hpcrun_ompt_get_task_frame(2)->reenter_runtime_frame));
  return;
  // TODO: consider this after see what is wrong with previous

  // ancestor region that is one level above me
  int nested_level_ancestor = top_index - 1;
  current_notification = region_stack[nested_level_ancestor];
  // it is possible that I am not the master of this region
  if(current_notification->region_data == not_master_region) {
    printf("I am not the master of the ancestor\n");
    return;
  }
  // it is possible that I have already resolve this region
  // because I took a sample before nested region is created
  if(current_notification->region_data->call_path != NULL) {
    printf("Ancestor level: %d is resolved.\n", nested_level_ancestor);
    return;
  }

  printf("Mozda bi mogao nesto prikupiti i ovde?\n");


//  printf("===================== <Start %d> ===============\n", top_index+1);
//  ompt_frame_t* current_frame;
//  ompt_frame_t* ancestor_frame;
//
//  int i;
//  int level;
//  for(i = top_index; i>=0; i--) {
//    level = top_index - i;
//    current_frame = hpcrun_ompt_get_task_frame(top_index - i);
//
//    indent_helper(level);
//    printf("current reenter: %lu, current exit: %lu\t", (uint64_t)current_frame->reenter_runtime_frame,
//           (uint64_t)current_frame->exit_runtime_frame);
//    if(i == 0) {
//      ancestor_frame = hpcrun_ompt_get_task_frame(top_index - i + 1);
//
//    }
//    printf("\n");
//
//  }
//  printf("BT Inner: %lu, BT Outer: %lu\n", (uint64_t)bt->begin->cursor.sp, (uint64_t)bt->last->cursor.sp);
//  printf("===================== <End %d> ===============\n\n\n", top_index+1);


//  int cnt_bt = 0;
//  int cnt_cct = 0;
//  frame_t **bt_outer = &bt->last;
//  frame_t **bt_inner = &bt->begin;
//  cct_node_t *current = cct;
//  frame_t *it = NULL;
//
//  for(it = *bt_inner; it <= *bt_outer; it++) {
//    cnt_bt++;
//  }
//
//  while(current) {
//    cnt_cct++;
//    current = hpcrun_cct_parent(current);
//  }
//
//  printf("BT: %d\tCCT: %d\n", cnt_bt, cnt_cct);





//  // FIXME: check if you set everything fine with this previous thing
//  cct_node_t* current_cct = cct;
//  cct_node_t* previous = current_cct;
//  cct_node_t* end_cct = NULL;
//  cct_node_t* begin_cct = NULL;
//
//  frame_t **bt_outer = &bt->last;
//  frame_t **bt_inner = &bt->begin;
//  frame_t *it = NULL;
//  int i = 0;
//  // the stack is empty, nothing needs to be done
//  if(is_empty_region_stack()){
//    // this happened
//    //printf("Stack is empty\n");
//    return;
//  }
//
//  // maybe the first region is not_master_region or resolved
//  ompt_notification_t* current_notification = top_region_stack();
//  if(current_notification->region_data == not_master_region
//     || current_notification->region_data->call_path != NULL){
//    // this happened
//    //printf("The first region has alredy been resolved\n");
//    return;
//  }
//
//  ompt_frame_t *frame0 = hpcrun_ompt_get_task_frame(i);
//  // FIXME: check if you implement right corner cases
//  // no innermost frame has ben set
//  if(!frame0){
//    printf("No first frame");
//    return;
//  }
//
//  // frame has been set, but not filled in
//  if((frame0->reenter_runtime_frame == 0) && (frame0->exit_runtime_frame == 0)){
//    // this happened
//    //printf("Both reenter and exit runtime frames are zero\n");
//    return;
//  }
//
//  if (frame0->exit_runtime_frame &&
//        (((uint64_t) frame0->exit_runtime_frame) < ((uint64_t) (*bt_inner)->cursor.sp))) {
//    // however, exit_runtime_frame points beyond the top of stack. the final call
//    // to user code hasn't been made yet. ignore this frame.
//    printf("The final call to the user code has not been set\n");
//    return;
//  }
//  int current_region_frame_found = 0;
//  if(frame0->reenter_runtime_frame) {
//    // this happened
//    //printf("Sample was taken inside runtime code\n");
//    for(it = *bt_inner; it <= *bt_outer && current_cct; it++, current_cct = hpcrun_cct_parent(current_cct)){
//      if((uint64_t)(it->cursor.sp) >= (uint64_t)frame0->reenter_runtime_frame){
//        // this has happened
//        //printf("Found the frame of the first region\n");
//        current_region_frame_found = 1;
//        break;
//      }
//      previous = current_cct;
//    }
//    if(!current_region_frame_found){
//      printf("The first region could not be found\n");
//      return;
//    }
//  }
//
//  // FIXME: probably we need to check if the exit runtime frame has been set
//
//  // we found the top frame of the region which correspond to the end cct of region callpath
//  if(current_region_frame_found) {
//    // this happened
//    //printf("Provide frame for the parent\n");
//    end_cct = current_cct;
//
//    if(top_index <= 0){
//      // happened
//      //printf("Only one region, and I need to provide parent\n");
//      begin_cct = top_cct(current_cct);
//      //printf("BEGIN: %p\tEND: %p\n", begin_cct, end_cct);
//      // TODO: copy prefix
//      return;
//    }
//    // we need to find the parent frame
//    int parent_found = 0;
//    if(!frame0 ->exit_runtime_frame){
//      printf("We cannot find the parent, because not have previous exit from runtime\n");
//      return;
//    }
//
//    for(it; it <=*bt_outer && current_cct; it++, current_cct = hpcrun_cct_parent(current_cct)) {
//      if((uint64_t)(it->cursor.sp) >= (uint64_t)frame0->exit_runtime_frame){
//        // this has happened
//        //printf("Found the frame of the first region\n");
//        parent_found = 1;
//        // one beyond the runtime frame is user code
//        it--;
//        // this should be fine
//        current_cct = previous;
//        break;
//      }
//      previous = current_cct;
//    }
//    if(parent_found){
//      // set parent beginning frame
//      begin_cct = current_cct;
//      // this happened
//      // printf("Find the parent of the first region\n");
//      // TODO: copy prefix
//    }else {
//      // FIXME: Why this happens???
//      //printf("This should not happen. You need to find parent frame %lu %lu\n", (uint64_t)frame0->exit_runtime_frame, (uint64_t)(it->cursor.sp));
//      return;
//    }
//
//    // go one frame upper inside runtime frames
//    it++;
//    previous = current_cct;
//    current_cct = hpcrun_cct_parent(current_cct);
//  } else {
//    // sample has been taken inside user code, which means that we should
//    // skip all user frames
//
//    if(!frame0->exit_runtime_frame){
//      printf("Sample is caught inside user code, but no exit runtime frame\n");
//      return;
//    }
//
//    for(it = *bt_inner; it <=*bt_outer && current_cct; it++, current_cct = hpcrun_cct_parent(current_cct)) {
//      // go up through stack frames until foes into runtime frames
//      if((uint64_t)(it->cursor.sp) >= (uint64_t)frame0->exit_runtime_frame){
//        // this happened
//        //printf("Found the frame of the first region\n");
//        break;
//      }
//      previous = current_cct;
//    }
//  }
//  // should we go from the innermost region, or one above it
//  int ancestor_level = (current_region_frame_found) ? 1 : 0;
//  ompt_region_data_t* current_region = NULL;
//  ompt_frame_t* frame_current;
//  ompt_frame_t* frame_ancestor;
//  for(i = top_index - ancestor_level; i >= 0; i--, ancestor_level++) {
//    current_notification = region_stack[i];
//    current_region = current_notification->region_data;
//    if(current_region == not_master_region || current_region->call_path != NULL){
//      // this happened
//      //printf("Everything is resolved fine\n");
//      return;
//    }
//
//    frame_current = hpcrun_ompt_get_task_frame(ancestor_level);
//    for(; it <=*bt_outer && current_cct; it++, current_cct = hpcrun_cct_parent(current_cct)) {
//      // go up through stack frames until foes into runtime frames
//      if((uint64_t)(it->cursor.sp) >= (uint64_t)frame_current->reenter_runtime_frame){
//        // this happened
//        //printf("I shoould somehow find something here\n");
//        break;
//      }
//      previous = current_cct;
//    }
//    // end_cct is just above reenter runtime frame
//    end_cct = current_cct;
//
//    // if we are at the bottom of the stack, all remainit cct-a are path
//    if(i == 0) {
//      begin_cct = top_cct(current_cct);
//      // this happened
//      //printf("I am the top\n");
//      // TODO: copy the prefix
//      return;
//    }
//
//    frame_ancestor = hpcrun_ompt_get_task_frame(ancestor_level+1);
//    int found_ancestor = 0;
//    for(; it <=*bt_outer && current_cct; it++, current_cct = hpcrun_cct_parent(current_cct)) {
//      // go up through stack frames until foes into runtime frames
//      if((uint64_t)(it->cursor.sp) >= (uint64_t)frame_ancestor->exit_runtime_frame){
//        found_ancestor = 1;
//        break;
//      }
//      previous = current_cct;
//    }
//    if(found_ancestor){
//      begin_cct = previous;
//      it++;
//      // this happened
//      //printf("I also find and the ancestor\n");
//      // TODO: copy prefix
//    } else {
//      // FIXME: Why this happened?
//      //printf("I don't understand this\n");
//      return;
//    }
//  }
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




