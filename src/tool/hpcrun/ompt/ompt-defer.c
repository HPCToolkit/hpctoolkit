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



// FIXME: move this function at better place

int get_stack_index(ompt_region_data_t *region_data) {
  int i;
  for (i = top_index; i>=0; i--) {
    if(region_stack[i].notification->region_data->region_id == region_data->region_id) {
      return i;
    }
  }
  return -1;
}

/*
 * r0
 *   r1
 *     r2 -> r3 (thread 123) -> r4 -> r5
 *        -> r6 -> r7 -> r8
 *     r9 (thread 123) -> r10
 * */


ompt_notification_t*
help_notification_alloc(ompt_region_data_t *region_data)
{
  ompt_notification_t *notification = hpcrun_ompt_notification_alloc();
  notification->region_data = region_data;
  notification->threads_queue = &threads_queue;
  return notification;
}

void
swap_and_free(ompt_region_data_t* region_data)
{

  int depth = region_data->depth;
  ompt_notification_t *notification;
  region_stack_el_t *stack_element;

  // If notification at depth index of the stack
  // does not have initialize next pointer, that means
  // that is not enqueue anywhere and is not in the freelist,
  // which means we can free it here.
  stack_element = &region_stack[depth];
  notification = stack_element->notification;
  // we can free notification either if the thread is the master of the region
  // or the thread did not take a sample inside region
  if (notification && (stack_element->team_master || !stack_element->took_sample)) {
    hpcrun_ompt_notification_free(notification);
  }

  // add place on the stack for the region
  notification = help_notification_alloc(region_data);
  region_stack[depth].notification = notification;
  // thread could be master only for region_data, see explanation
  // given in comment below
  region_stack[depth].team_master = 0;
  region_stack[depth].took_sample = 0;

  // I previosly use this as a condition to free notification,
  // which is bad and the explanation is below
  // Notification can be at the end of the queue
  // and this condition does not check thath
  //    if (OMPT_BASE_T_GET_NEXT(notification) == NULL
  //        && wfq_get_next(OMPT_BASE_T_STAR(notification)) == NULL) {

}

void
add_region_and_ancestors_to_stack(ompt_region_data_t *region_data, bool team_master)
{

  if (!region_data) {
    printf("*******************This is also possible. ompt-defer.c:394");
    return;
  }

  ompt_region_data_t *current = region_data;
  int level = 0;
  int depth;
  ompt_notification_t *notification;
  // up through region stack until first region which is on the stack
  // if none of region is on the stack, then stack will be completle changed
  while (current) {
    depth = current->depth;
    // found region which is on the stack
    if (depth <= top_index &&
        region_stack[depth].notification->region_data->region_id == current->region_id) {
      break;
    }
    swap_and_free(current);
    // get the parent
    current = hpcrun_ompt_get_region_data(++level);
  }

  // region_data is the new top of the stack
  top_index = region_data->depth;
  // FIXME vi3: should check if this is right
  // If the stack content does not corresponds to ancestors of the region_data,
  // then thread could only be master of the region_data, but not to its ancestors.
  // Values of argument team_master says if the thread is the master of region_data
  region_stack[top_index].team_master = team_master;

}


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
    new = hpcrun_cct_insert_addr(region_stack[top_index - 1].notification->unresolved_cct,
                                 &(ADDR2(UNRESOLVED, region_data->region_id)));
  }

  return new;
}



// vi3 temp check
int alredy_in_list(ompt_region_data_t *region_data) {
    ompt_trl_el_t* current = registered_regions;
    while(current){
        if(current->region_data == region_data){
            return 1;
        }
        current = (ompt_trl_el_t*)OMPT_BASE_T_GET_NEXT(current);
    }

    return 0;
}
int print_list() {
    ompt_trl_el_t* current = registered_regions;
    int i = 0;
    while(current){
        printf("%p, %lx, i: %d, master:%d\n", &threads_queue, current->region_data->region_id, i, TD_GET(master));
        current = (ompt_trl_el_t*)OMPT_BASE_T_GET_NEXT(current);
        i++;
    }

    return 0;
}



void
register_to_region(ompt_notification_t* notification){
 ompt_region_data_t* region_data = notification->region_data;

 // create notification and enqueu to region's queue
 OMPT_BASE_T_GET_NEXT(notification) = NULL;
 // register thread to region's wait free queue
 wfq_enqueue(OMPT_BASE_T_STAR(notification), &region_data->queue);

  // increment the number of unresolved regions
  unresolved_cnt++;

//  printf("THREAD: %p, REGION_ID: %lx\n", &threads_queue, region_data->region_id);
}

ompt_notification_t*
add_notification_to_stack(ompt_region_data_t* region_data) {
    ompt_notification_t* notification = hpcrun_ompt_notification_alloc();
    notification->region_data = region_data;
    notification->threads_queue = &threads_queue;
    // push to stack
    push_region_stack(notification, 0, 0);
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

int
get_took_sample_parent_index()
{
  int i;
  for (i = top_index; i >= 0; i--) {
    if (region_stack[i].took_sample) {
      return i;
    }
  }
  return -1;
}


void
register_to_all_regions(){


  // find ancestor on the stack in which we took a sample
  // a go until the top of the stack
  int start_register_index = get_took_sample_parent_index() + 1;

  // mark that all descendants of previously mentioned ancestor took a sample
  // for all descendants where thread is not the master in, register for descendant callpath
  int i;
  region_stack_el_t *current_el;
  cct_node_t *parent_cct;
  cct_node_t *new_cct;
  for (i = start_register_index; i <=top_index; i++) {
    current_el = &region_stack[i];
    // mark that we took sample
    current_el->took_sample = true;
    if (!current_el->team_master) {
      // register for region's call path if not the master
      register_to_region(current_el->notification);
      // add unresolved cct at some place underneath thread root
      // find parent of new_cct
      parent_cct = (i == 0) ? hpcrun_get_thread_epoch()->csdata.thread_root
              : region_stack[i-1].notification->unresolved_cct;

      if (current_el->notification->region_data->region_id == 0) {
        printf("*************You have made big mistake\n");
      }
      // insert cct as child of the parent_cct
      new_cct =
              hpcrun_cct_insert_addr(parent_cct,
                                     &(ADDR2(UNRESOLVED, current_el->notification->region_data->region_id)));
      // remebmer cct
      current_el->notification->unresolved_cct = new_cct;

      cct_not_master_region = current_el->notification->unresolved_cct;

    }
  }


}

// insert a path to the root and return the path in the root
cct_node_t*
hpcrun_cct_insert_path_return_leaf_tmp(cct_node_t *root, cct_node_t *path)
{
    if(!path) return root;
    cct_node_t *parent = hpcrun_cct_parent(path);
    if (parent) {
      root = hpcrun_cct_insert_path_return_leaf_tmp(root, parent);
    }
    return hpcrun_cct_insert_addr(root, hpcrun_cct_addr(path));
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

  if (parent_unresolved_cct == NULL) {
      printf("*******************PARENT UNRESOLVED CCT IS MISSING! OLD_HEAD_NOTIFICATION: %p\n", old_head);
      //unresolved_cnt--;
      return 0;
  }

  if(region_data->call_path == NULL) {
    printf("*******************This is very bad!\n");
    return 0;
  }

  // prefix should be put between unresolved_cct and parent_unresolved_cct
  cct_node_t *prefix = NULL;
  cct_node_t *region_call_path = region_data->call_path;

  // FIXME: why hpcrun_cct_insert_path_return_leaf ignores top cct of the path
  // when had this condtion, once infinity happen
  if (parent_unresolved_cct == hpcrun_get_thread_epoch()->csdata.thread_root) {
    // from initial region, we should remove the first one
    prefix = hpcrun_cct_insert_path_return_leaf(parent_unresolved_cct, region_call_path);
  } else {
    // for resolving inner region, we should consider all cct nodes from prefix
    prefix = hpcrun_cct_insert_path_return_leaf_tmp(parent_unresolved_cct, region_call_path);
  }



//  printf("THREAD: %p, RESOLVING: %lx, UNR_CCT: %lx, IS_PARENT_THREAD_ROOT: %d, PARENT_CCT: %lx, PREFIX: %lx\n",
//         &threads_queue,
//         region_data->region_id,
//         hpcrun_cct_addr(unresolved_cct)->ip_norm.lm_ip,
//         hpcrun_get_thread_epoch()->csdata.thread_root == parent_unresolved_cct,
//         hpcrun_cct_addr(parent_unresolved_cct)->ip_norm.lm_ip,
//         hpcrun_cct_addr(prefix)->ip_norm.lm_ip
//         );

  if (prefix == NULL) {
    printf("**************SOMETHING IS BAD WHILE RESOLVING... REION_ID: %lx, REGION_CALLPATH: %p, MASTER: %d,"
           " UNRESOLVED_CCT_IP: %lx, UNRESOLVED_PARENT_CCT: %p, THREAD: %p\n",
           region_data->region_id, region_call_path, TD_GET(master), hpcrun_cct_addr(unresolved_cct)->ip_norm.lm_ip,
           parent_unresolved_cct, &threads_queue
    );
  }

  // prefix node should change the unresolved_cct
  hpcrun_cct_merge(prefix, unresolved_cct, merge_metrics, NULL);
  // delete unresolved_cct from parent
  hpcrun_cct_delete_self(unresolved_cct);
  // ==================================


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
  //printf("SUCCESSFULLY RESOLVED REGION_ID: %lx, THREAD: %p\n", region_data->region_id, &threads_queue);
  return 1;
}

void resolving_all_remaining_context(){
  // resolve all remaining regions
  //printf("master: %d, unresolved_cnt: %d\n", TD_GET(master), unresolved_cnt);
  while(unresolved_cnt) {
    try_resolve_one_region_context();
  }
  // FIXME vi3: find all memory leaks
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
  if(!TD_GET(master) && stack_index && region_stack[stack_index - 1].notification->region_data == not_master_region){
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

int
prefix_length(cct_node_t *bottom_prefix, cct_node_t *top_prefix)
{
  int len = 0;
  cct_node_t *current = bottom_prefix;
  while (current) {
    len++;
    if (current == top_prefix) {
          break;
    }
    current = hpcrun_cct_parent(current);
  }

  return len;
}

void
provide_callpath_for_regions_if_needed(backtrace_info_t *bt, cct_node_t *cct) {
  if (bt->partial_unwind) {
    printf("---------------------Something is not good here\n");
  }

  // not regions on the stack, should mean that we are not inside parallel region
  if (is_empty_region_stack())
    return;

  // if thread is not the master of the region, or the region is resolved, then just return
  ompt_notification_t *current_notification = top_region_stack()->notification;
  if (this_thread_cannot_resolve_region(current_notification)) {
    // this happened
    //printf("I am not responsible for this region\n");
    return;
  }

  int index = 0;
  ompt_frame_t *current_frame = hpcrun_ompt_get_task_frame(0);
  if (!current_frame) {
    return;
  }
  frame_t *bt_inner = bt->begin;
  frame_t *bt_outer = bt->last;
  // frame iterator
  frame_t *it = bt_inner;

  //printf("Inner has smaller address: %d\n", UINT64_T(bt_inner->cursor.sp) < UINT64_T(bt_outer->cursor.sp));


  cct_node_t *bottom_prefix = NULL;
  cct_node_t *top_prefix = NULL;
  cct_node_t *prefix = NULL;

  if (UINT64_T(current_frame->reenter_runtime_frame) == 0
    && UINT64_T(current_frame->exit_runtime_frame) != 0) {
    // thread take a sample inside user code of the parallel region
    // this region is the the innermost

    // this has happened
    //printf("First time in user code of the parallel region\n");

    it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
    if (it == NULL) {

        return;
    }
    // this happened
    //printf("One above exit\n");

  } else if (UINT64_T(current_frame->reenter_runtime_frame) <= UINT64_T(bt_inner->cursor.sp)
             && UINT64_T(bt_inner->cursor.sp) <= UINT64_T(current_frame->exit_runtime_frame)) { // FIXME should put =
    // thread take a simple in the region which is not the innermost
    // all innermost regions have been finished

    // this happened
    //printf("Second time in user code of the parallel region\n");

    it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
    if (it == NULL) {
        // this happened once for the thread da it is not TD_GET(master)

        return;
    }
    // this happened
    //printf("One above exit...\n");

  } else if (UINT64_T(bt_inner->cursor.sp) < UINT64_T(current_frame->reenter_runtime_frame)) {
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
  } else if (UINT64_T(current_frame->exit_runtime_frame) == 0
             && UINT64_T(bt_inner->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)) {
    //ompt_frame_t* parent = hpcrun_ompt_get_task_frame(1);
    //printf("I did not cover. BT_INNNER: %lu\tEXIT: %lu\tREENTER: %lu\tREGIONS: %d\tMASTER: %d\tPARENT FRAME: %p\n",
    //       UINT64_T(bt_inner->cursor.sp), UINT64_T(current_frame->exit_runtime_frame),
    //       UINT64_T(current_frame->reenter_runtime_frame), top_index+1, TD_GET(master), parent);

    // FIXME: the master thread is probably inside implicit task
    // this happened
//    printf("Master thread is inside implicit task\n");



    // FIXME vi3: this happened in the first region when master not took sample

    bottom_prefix = get_cct_from_prefix(cct, index);
    top_prefix = top_cct(cct);

    prefix = copy_prefix(top_prefix, bottom_prefix);
    if (!prefix) {
      printf("Should this ever happen?\n");
      return;
    }
    current_notification->region_data->call_path = prefix;

    return;
  } else {
    printf("Something that has not been cover\n");

    return;
  }


  int frame_level = 0;
  int stack_index = top_index;


  // at the beginning of the loop we should be one frame above the end_frame
  // which means that we are inside user code of the parent region or the initial implicit task
  // if the current thread is the initial master
  // while loop
  for (;;) {
    frame_level++;
    current_frame = hpcrun_ompt_get_task_frame(frame_level);
    if (!current_frame) {
      printf("************************This makes problem\n");
      return;
    }

    // we should be inside the user code of the parent parallel region
    // but let's check that
    if (UINT64_T(it->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)) {
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
    if (!bottom_prefix) {
      // FIXME vi3: Why this happened at morenested.c example
      printf("Should this ever happened\n");
      return;
    }

    // we found the end of the parent, next thing is to find the beginning of the parent
    // if this is the last region on the stack, then all remaining cct should be in the prefix
    if (TD_GET(master) && stack_index == 0) {
      top_prefix = top_cct(bottom_prefix);
      prefix = copy_prefix(top_prefix, bottom_prefix);
      if (!prefix) {
        printf("Should this ever happen?\n");
        return;
      }
      current_notification->region_data->call_path = prefix;
      return;

//        print_prefix_info("", prefix, current_notification->region_data, stack_index, bt, cct);

          // vi3: debug
//          int len = prefix_length(bottom_prefix, top_prefix);
//    printf("LAST LENGTH: %d, REGION: %lx, TOP_LM_IP: %lx, BOTTOM_LM_IP: %lx\n",
//             len, region_stack[stack_index]->region_data->region_id,
//             hpcrun_cct_addr(top_prefix)->ip_norm.lm_ip,
//             hpcrun_cct_addr(bottom_prefix)->ip_norm.lm_ip);


    }

    // we are inside user code of outside region
    // should find one frame below exit_runtime_frame
    it = first_frame_below(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
    // FIXME: vi3 What is this commented?
    //top_prefix = get_cct_from_prefix(cct, TD_GET(master) ? index + 1 : index);


    top_prefix = get_cct_from_prefix(cct, index);
    // FIXME: unresolved should not be top_prefix, get its child,
    // I think that this happens when the thread is not initial master
    if (hpcrun_cct_addr(top_prefix)->ip_norm.lm_id == (uint16_t) UNRESOLVED) {
      // index - 1 is the index of child of the top_prefix
      top_prefix = get_cct_from_prefix(cct, index - 1);
    }


    prefix = copy_prefix(top_prefix, bottom_prefix);
    if (!prefix) {
      printf("What to do now?\n");
      return;
    }
    // vi3: debug
//    int len = prefix_length(bottom_prefix, top_prefix);
//  printf("LENGTH: %d, REGION: %lx, TOP_LM_IP: %lx, BOTTOM_LM_IP: %lx\n",
//         len, region_stack[stack_index]->region_data->region_id,
//          hpcrun_cct_addr(top_prefix)->ip_norm.lm_ip,
//          hpcrun_cct_addr(bottom_prefix)->ip_norm.lm_ip);


    current_notification->region_data->call_path = prefix;
    //print_prefix_info("", prefix, current_notification->region_data, stack_index, bt, cct);

    // next thing to do is to get parent region notification from stack
    stack_index--;
    if (stack_index < 0) {
        printf("***********Something bad is hapenning\n");
        return;
    }
    current_notification = region_stack[stack_index].notification;
    if (this_thread_cannot_resolve_region(current_notification)) {
      // this happened
      //printf("THREAD FINISHED EVERYTHING\n");
      return;
    }
    // go to parent region frame and do everything again
    it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);


  }

}

void
provide_callpath_for_end_of_the_region(backtrace_info_t *bt, cct_node_t *cct) {
    if (bt->partial_unwind) {
        printf("---------------------Something is not good here\n");
    }

    int index = 0;
    ompt_frame_t *current_frame = hpcrun_ompt_get_task_frame(0);
    if (!current_frame) {
      return;
    }
    frame_t *bt_inner = bt->begin;
    frame_t *bt_outer = bt->last;
    // frame iterator
    frame_t *it = bt_inner;


    cct_node_t *bottom_prefix = NULL;
    cct_node_t *top_prefix = NULL;
    cct_node_t *prefix = NULL;

    if (UINT64_T(current_frame->reenter_runtime_frame) == 0
        && UINT64_T(current_frame->exit_runtime_frame) != 0) {
        // thread take a sample inside user code of the parallel region
        // this region is the the innermost

        // this has happened
        //printf("First time in user code of the parallel region\n");

//        it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
//        if (it == NULL) {
//
//            return;
//        }
        // this happened
        //printf("One above exit\n");


        printf("MASTER: %d, FIRST\n", TD_GET(master));
    } else if (UINT64_T(current_frame->reenter_runtime_frame) <= UINT64_T(bt_inner->cursor.sp)
               && UINT64_T(bt_inner->cursor.sp) <= UINT64_T(current_frame->exit_runtime_frame)) { // FIXME should put =
        // thread take a simple in the region which is not the innermost
        // all innermost regions have been finished

        // this happened
        //printf("Second time in user code of the parallel region\n");

//        it = first_frame_above(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
//        if (it == NULL) {
//            // this happened once for the thread da it is not TD_GET(master)
//
//            return;
//        }
        // this happened
        //printf("One above exit...\n");




        bottom_prefix = get_cct_from_prefix(cct, index);
        it = first_frame_below(it, bt_outer, UINT64_T(current_frame->exit_runtime_frame), &index);
        top_prefix = get_cct_from_prefix(cct, index);

        // FIXME: unresolved should not be top_prefix, get its child,
        // I think that this happens when the thread is not initial master
        if (hpcrun_cct_addr(top_prefix)->ip_norm.lm_id == (uint16_t) UNRESOLVED) {
            // index - 1 is the index of child of the top_prefix
            top_prefix = get_cct_from_prefix(cct, index - 1);
        }

        prefix = copy_prefix(top_prefix, bottom_prefix);
        if (!prefix) {
            printf("-----------------Something is wrong\n");
            return;
        }
        ending_region->call_path = prefix;


    } else if (UINT64_T(bt_inner->cursor.sp) < UINT64_T(current_frame->reenter_runtime_frame)) {
        // take a sample inside the runtime
        // this happened
        //printf("Sample has been taken inside runtime frame\n");

        printf("MASTER: %d, THIRD\n", TD_GET(master));

//        return;
    } else if (UINT64_T(current_frame->reenter_runtime_frame) == 0
               && UINT64_T(current_frame->exit_runtime_frame) == 0) {
        // FIXME: check what this means
        // this happened
        //printf("Both reenter and exit are zero\n");

        // this happened a couple of time for not TD_GET(master) threads

        printf("MASTER: %d, FOURTH\n", TD_GET(master));


//        return;
    } else if (UINT64_T(current_frame->exit_runtime_frame) == 0
               && UINT64_T(bt_inner->cursor.sp) >= UINT64_T(current_frame->reenter_runtime_frame)) {
        //ompt_frame_t* parent = hpcrun_ompt_get_task_frame(1);
        //printf("I did not cover. BT_INNNER: %lu\tEXIT: %lu\tREENTER: %lu\tREGIONS: %d\tMASTER: %d\tPARENT FRAME: %p\n",
        //       UINT64_T(bt_inner->cursor.sp), UINT64_T(current_frame->exit_runtime_frame),
        //       UINT64_T(current_frame->reenter_runtime_frame), top_index+1, TD_GET(master), parent);

        // FIXME: the master thread is probably inside implicit task
        // this happened
        //printf("Master thread is inside implicit task\n");



        // FIXME vi3: this happened in the first region when master not took sample

        bottom_prefix = get_cct_from_prefix(cct, index);
        top_prefix = top_cct(cct);

        // copy prefix
        prefix = copy_prefix(top_prefix, bottom_prefix);
        if (!prefix) {
          printf("Should this ever happen?\n");
          return;
        }

        ending_region->call_path = prefix;

        return;

    } else {
//        printf("Something that has not been cover\n");

//        return;

        printf("MASTER: %d, SIXTH\n", TD_GET(master));

    }


}

void
tmp_end_region_resolve(ompt_notification_t *notification, cct_node_t* prefix)
{

#if 1
  // ================================== resolving part
  cct_node_t *unresolved_cct = notification->unresolved_cct;

  if (prefix == NULL) {
    printf("*******************PREFIX CCT IS MISSING! OLD_HEAD_NOTIFICATION: %p\n", notification);
    //unresolved_cnt--;
    return;
  }

  // prefix node should change the unresolved_cct
  hpcrun_cct_merge(prefix, unresolved_cct, merge_metrics, NULL);
  // delete unresolved_cct from parent
  hpcrun_cct_delete_self(unresolved_cct);
  // ==================================
#endif

  // free notification
  //hpcrun_ompt_notification_free(old_head);
}