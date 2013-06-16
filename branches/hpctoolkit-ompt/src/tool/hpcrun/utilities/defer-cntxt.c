/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/trace.h>
#include <hpcrun/cct/cct.h>
#include <messages/messages.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/metrics.h>
#include <utilities/defer-cntxt.h>
#include <utilities/defer-write.h>
#include <hpcrun/unresolved.h>
#include <hpcrun/write_data.h>

#include <ompt.h>

/******************************************************************************
 * type definition 
 *****************************************************************************/

struct record_t {
  uint64_t region_id;
  uint64_t use_count;
  cct_node_t *node;
  struct record_t *left;
  struct record_t *right;
};

int omp_get_level(void);
int omp_get_thread_num(void);

uint64_t is_partial_resolve(cct_node_t *prefix);

/******************************************************************************
 * splay tree operation for record map *
 *****************************************************************************/
static struct record_t *record_tree_root = NULL;
static spinlock_t record_tree_lock = SPINLOCK_UNLOCKED;

static struct record_t *
r_splay(struct record_t *root, uint64_t key)
{
  REGULAR_SPLAY_TREE(record_t, root, key, region_id, left, right);
  return root;
}
static struct record_t *
r_splay_lookup(uint64_t id)
{
  spinlock_lock(&record_tree_lock);
  record_tree_root = r_splay(record_tree_root, id);
  if(!record_tree_root || record_tree_root->region_id != id) {
    spinlock_unlock(&record_tree_lock);
    return NULL;
  }
  spinlock_unlock(&record_tree_lock);
  return record_tree_root;
}

static void
r_splay_insert(struct record_t *node)
{
  uint64_t region_id = node->region_id;

  node->left = node->right = NULL;

  spinlock_lock(&record_tree_lock);
  if(record_tree_root != NULL) {
    record_tree_root = r_splay(record_tree_root, region_id);

    if(region_id < record_tree_root->region_id) {
      node->left = record_tree_root->left;
      node->right = record_tree_root;
      record_tree_root->left = NULL;
    } else if(region_id > record_tree_root->region_id) {
      node->left = record_tree_root;
      node->right = record_tree_root->right;
      record_tree_root->right = NULL;
    } else {
      assert(0);
    }
  }
  record_tree_root = node;
  spinlock_unlock(&record_tree_lock);
}

static struct record_t *
r_splay_delete(uint64_t region_id)
{
  TMSG(DEFER_CTXT, "delete region %d", region_id);
// no need to use spin lock in delete because we only call delete in update function where
// tree is already locked
  struct record_t *result = NULL;

  if(record_tree_root == NULL) {
    return NULL;
  }

  record_tree_root = r_splay(record_tree_root, region_id);
  if(region_id != record_tree_root->region_id) {
    return NULL;
  }

  result = record_tree_root;

  if(record_tree_root->left == NULL) {
    record_tree_root = record_tree_root->right;
    return result;
  }

  record_tree_root->left = r_splay(record_tree_root->left, region_id);
  record_tree_root->left->right = record_tree_root->right;
  record_tree_root = record_tree_root->left;
  return result;
}

// if found the record, return true. Otherwise, return false
static bool
r_splay_count_update(uint64_t region_id, uint64_t val)
{
  TMSG(DEFER_CTXT, "update region %d with %d", region_id, val);
  spinlock_lock(&record_tree_lock);
  record_tree_root = r_splay(record_tree_root, region_id);

  if(!record_tree_root){
    spinlock_unlock(&record_tree_lock);
    return false;
  }
  if(record_tree_root->region_id != region_id) {
    spinlock_unlock(&record_tree_lock);
    return false;
  } else {
    record_tree_root->use_count += val;
    TMSG(DEFER_CTXT, "I am value %d (%d) for region %d", record_tree_root->use_count, val, region_id);
    if((record_tree_root->use_count == 0)) {
      TMSG(DEFER_CTXT, "I am here for delete");
      r_splay_delete(record_tree_root->region_id);
    }
  }
  spinlock_unlock(&record_tree_lock);
  return true;
}
/******************************************************************************
 * private operations 
 *****************************************************************************/

struct record_t *
new_record(uint64_t region_id)
{
  struct record_t *record;
  record = (struct record_t *)hpcrun_malloc(sizeof(struct record_t));
  record->region_id = region_id;
  record->use_count = 0;
  record->node = NULL;
  record->left = NULL;
  record->right = NULL;

  return record;
}


void
gather_context(struct record_t *record)
{
  cct_node_t *node;
  ucontext_t uc;
  getcontext(&uc);
  //
  // for side thread or master thread in the nested region, unwind to the dummy root 
  // with outer most region attached to the tbd root
  //
  if(! TD_GET(master)) { //the sub-master thread in nested regions
    omp_arg_t omp_arg;
    omp_arg.tbd = false;
    omp_arg.context = NULL;
    if(TD_GET(region_id) > 0) {
      omp_arg.tbd = true;
      omp_arg.region_id = TD_GET(region_id);
    }
    node = hpcrun_sample_callpath(&uc, 0, 0, 1, 1, (void *)&omp_arg).sample_node;
    TMSG(DEFER_CTXT, "unwind the callstack for region %d to %d", record->region_id, TD_GET(region_id));
  } else {
    // for master thread in the outer-most region, a normal unwind to the process stop 
    node = hpcrun_sample_callpath(&uc, 0, 0, 1, 1, NULL).sample_node;
    TMSG(DEFER_CTXT, "unwind the callstack for region %d", record->region_id);
  }

#ifdef GOMP
      cct_node_t *sibling = NULL;
      if(node)
        sibling = hpcrun_cct_insert_addr(hpcrun_cct_parent(node), 
			    &(ADDR2(hpcrun_cct_addr(node)->ip_norm.lm_id, 
				hpcrun_cct_addr(node)->ip_norm.lm_ip-5L)));
      record->node = sibling;
#else
      record->node = node;
#endif
}

#define EAGER_CONTEXT 1 // temporary -- johnmc

//
// only master and sub-master thread execute start_team_fn and end_team_fn
//
void start_team_fn(ompt_data_t *parent_task_data, ompt_frame_t *parent_task_frame,
		   ompt_parallel_id_t id)
{
  hpcrun_safe_enter();
  thread_data_t *td = hpcrun_get_thread_data();
  // mark the real master thread (the one with process stop)
  if (omp_get_level() == 1 && omp_get_thread_num() == 0) {
    td->master = 1;
  }
  // for a new record
  struct record_t *record = new_record((uint64_t)id);
#ifdef EAGER_CONTEXT
  if (hpcrun_trace_isactive()) gather_context(record);
#endif
  r_splay_insert(record);

#if 1
  if(td->master) {
    ;
  }
  else if(td->outer_region_id == 0) {
    td->outer_region_id = hpcrun_ompt_get_parallel_id(1);
    td->outer_region_context = NULL;
  }
  else {
    // check whether we should update the outer most id
    // if the outer-most region with td->outer_region_id is an outer region of current reigon,
    // then no need to update outer-most id in the td
    // else if it is not an outer region of the current region, we have to update the 
    // outer-most id 
    int i=0;
    uint64_t outer_id = 0;

    outer_id = hpcrun_ompt_get_parallel_id(i);
    while(outer_id > 0) {
      if(outer_id == td->outer_region_id) break;

      outer_id = hpcrun_ompt_get_parallel_id(++i);
    }
    if(outer_id == 0){
      td->outer_region_id = hpcrun_ompt_get_parallel_id(1);
      td->outer_region_context = 0;
    }
  }
#else
  if (! td->master) {
    td->outer_region_id = ompt_outermost_parallel_id();
    td->outer_region_context = 0;
  }
#endif

  hpcrun_safe_exit();
}

void end_team_fn(ompt_data_t *parent_task_data, ompt_frame_t *parent_task_frame,
		 ompt_parallel_id_t id)
{
  hpcrun_safe_enter();
  uint64_t region_id = id;
  struct record_t *record = r_splay_lookup(region_id);
  // insert resolved root to the corresponding record entry
  if(record && (record->region_id == region_id)) {
    if(record->use_count > 0) {
      if (record->node == NULL) gather_context(record);
    } else {
      r_splay_count_update(record->region_id, 0L);
    }
  }
  // FIXME: not using team_master but use another routine to 
  // resolve team_master's tbd. Only with tasking, a team_master
  // need to resolve itself
  if(ENABLED(OMPT_TASK_FULL_CTXT)) {
    TD_GET(team_master) = 1;
    thread_data_t* td = hpcrun_get_thread_data();
    resolve_cntxt_fini(td);
    TD_GET(team_master) = 0;
  }
  hpcrun_safe_exit();
}

int need_defer_cntxt()
{
  if (getenv("OMPT_LOCAL_VIEW")) return 0;

  // master thread does not need to defer the context
  if ((hpcrun_ompt_get_parallel_id(0) > 0) && !TD_GET(master)) {
    thread_data_t *td = hpcrun_get_thread_data();
    td->defer_flag = 1;
    return 1;
  }
  return 0;
}

//
// TODO: add trace correction info here
//
static void
merge_metrics(cct_node_t *a, cct_node_t *b, merge_op_arg_t arg)
{
  // if two nodes are the same, no need to merge
  if(a == b) return;
  cct_metric_data_t *mdata_a, *mdata_b;
  metric_desc_t *mdesc;
  metric_set_t* mset_a = hpcrun_get_metric_set(a);
  metric_set_t* mset_b = hpcrun_get_metric_set(b);
  if(!mset_a || !mset_b) return;
  int num = hpcrun_get_num_metrics();
  int i;
  for (i = 0; i < num; i++) {
    mdata_a = hpcrun_metric_set_loc(mset_a, i);
    mdata_b = hpcrun_metric_set_loc(mset_b, i);
    if(!mdata_a || !mdata_b) continue;
    mdesc = hpcrun_id2metric(i);
    if(mdesc->flags.fields.valFmt == MetricFlags_ValFmt_Int) {
      mdata_a->i += mdata_b->i;
    }
    else if(mdesc->flags.fields.valFmt == MetricFlags_ValFmt_Real) {
      mdata_a->r += mdata_b->r;
    }
    else {
      TMSG(DEFER_CTXT, "in merge_op: what's the metric type");
      monitor_real_exit(1);
    }
  }
}

uint64_t is_partial_resolve(cct_node_t *prefix)
{
  //go up the path to check whether there is a node with UNRESOLVED tag
  cct_node_t *node = prefix;
  while(node) {
    if(hpcrun_cct_addr(node)->ip_norm.lm_id == (uint16_t)UNRESOLVED)
      return (uint64_t)(hpcrun_cct_addr(node)->ip_norm.lm_ip);
    node = hpcrun_cct_parent(node);
  }
  return 0;
}

static void
omp_resolve(cct_node_t* cct, cct_op_arg_t a, size_t l)
{
  cct_node_t *prefix;
  thread_data_t *td = (thread_data_t *)a;
  uint64_t my_region_id = (uint64_t)hpcrun_cct_addr(cct)->ip_norm.lm_ip;
  TMSG(DEFER_CTXT, "try to resolve region %d", my_region_id);
  uint64_t partial_region_id = 0;
  if ((prefix = hpcrun_region_lookup(my_region_id))) {
    TMSG(DEFER_CTXT, "resolve region %d to %d", my_region_id, is_partial_resolve(prefix));
    // delete cct from its original parent before merging
    hpcrun_cct_delete_self(cct);
    TMSG(DEFER_CTXT, "delete from the tbd region %d", hpcrun_cct_addr(cct)->ip_norm.lm_ip);
    partial_region_id = is_partial_resolve(prefix);
    if(partial_region_id == 0) {
      prefix = hpcrun_cct_insert_path_return_leaf((td->core_profile_trace_data.epoch->csdata).tree_root, prefix);
    }
    else {
      prefix = hpcrun_cct_insert_path_return_leaf((td->core_profile_trace_data.epoch->csdata).unresolved_root, prefix);
      r_splay_count_update(partial_region_id, 1L);
    }
    // adjust the callsite of the prefix in side threads to make sure they are the same as
    // in the master thread. With this operation, all sides threads and the master thread
    // will have the unified view for parallel regions (this only works for GOMP)
    if(td->team_master) {
      hpcrun_cct_merge(prefix, cct, merge_metrics, NULL);
    }
    else {
      hpcrun_cct_merge(prefix, cct, merge_metrics, NULL);
      // must delete it when not used considering the performance
      r_splay_count_update(my_region_id, -1L);
      TMSG(DEFER_CTXT, "resolve region %d", my_region_id);
    }
  }
}

static void
omp_resolve_and_free(cct_node_t* cct, cct_op_arg_t a, size_t l)
{
  omp_resolve(cct, a, l);
}

void resolve_cntxt()
{
  hpcrun_safe_enter();
  uint64_t current_region_id = hpcrun_ompt_get_parallel_id(0); //inner-most region id
  cct_node_t* tbd_cct = (hpcrun_get_thread_epoch()->csdata).unresolved_root;
  thread_data_t *td = hpcrun_get_thread_data();
  uint64_t outer_region_id = 0;
  // no outer region in the first level
  if(omp_get_level() == 1 && (td->outer_region_id > 0)) td->outer_region_id = 0;
  if(td->outer_region_id > 0)
    outer_region_id = td->outer_region_id; // current outer region
  if(outer_region_id == 0) outer_region_id = current_region_id;
  // resolve the trees at the end of one parallel region
  if((td->region_id != outer_region_id) && (td->region_id != 0)){
    TMSG(DEFER_CTXT, "I want to resolve the context when I come out from region %d", td->region_id);
    hpcrun_cct_walkset(tbd_cct, omp_resolve_and_free, td);
  }
  // update the use count when come into a new omp region
  if((td->region_id != outer_region_id) && (outer_region_id != 0)) {
    // end_team_fn occurs at master thread, side threads may still 
    // in a barrier waiting for work. Now the master thread may delete
    // the record if no samples taken in it. But side threads may take 
    // samples in the waiting region (which has the same region id with
    // the end_team_fn) after the region record is deleted.
    // solution: consider such sample not in openmp region (need no
    // defer cntxt) 
    if(r_splay_count_update(outer_region_id, 1L))
      hpcrun_cct_insert_addr(tbd_cct, &(ADDR2(UNRESOLVED, outer_region_id)));
    else
      outer_region_id = 0;
  }
  // td->region_id represents the out-most parallel region id
  td->region_id = outer_region_id;

  hpcrun_safe_exit();
}

void resolve_cntxt_fini(thread_data_t *td)
{
  hpcrun_cct_walkset(td->core_profile_trace_data.epoch->csdata.unresolved_root, omp_resolve_and_free, td);
}

cct_node_t *
hpcrun_region_lookup(uint64_t id)
{
  struct record_t *record = r_splay_lookup(id);
  if (record) {
    return record->node;
  }
  return NULL;
}

