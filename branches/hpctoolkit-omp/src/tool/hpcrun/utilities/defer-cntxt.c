/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <papi.h>
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
#include <hpcrun/cct/cct.h>
#include <messages/messages.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

#include <omp.h>
#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/metrics.h>
#include <utilities/defer-cntxt.h>
#include <hpcrun/unresolved.h>
#include <hpcrun/write_data.h>

#include "/home/xl10/support/gcc-4.6.2/libgomp/libgomp_g.h"
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

struct entry_t {
  thread_data_t *td;
  struct entry_t *prev;
  struct entry_t *next;
  bool flag;
};

uint64_t is_partial_resolve(cct_node_t *prefix);
/******************************************************************************
 * entry variables and operations for delayed write *
 *****************************************************************************/

static struct entry_t *unresolved_list = NULL;
static struct entry_t *free_list = NULL;

static spinlock_t unresolved_list_lock = SPINLOCK_UNLOCKED;
static spinlock_t free_list_lock = SPINLOCK_UNLOCKED;

static struct entry_t *
new_dw_entry()
{
  struct entry_t *entry = NULL;
  spinlock_lock(&free_list_lock);
  if(free_list) {
    entry = free_list;
    free_list = free_list->next;
    if(free_list) free_list->prev = NULL;
    entry->prev = entry->next = NULL;
    entry->td = NULL;
    entry->flag = false;
  }
  else {
    entry = (struct entry_t*)hpcrun_malloc(sizeof(struct entry_t));
    entry->prev = entry->next = NULL;
    entry->td = NULL;
    entry->flag = false;
  }
  spinlock_unlock(&free_list_lock);
  return entry;
}

static void
delete_dw_entry(struct entry_t* entry)
{
#if 0
  // detach from the unresolved list
  spinlock_lock(&unresolved_list_lock);
  if(entry->prev) entry->prev->next = entry->next;
  if(entry->next) entry->next->prev = entry->prev;
  spinlock_unlock(&unresolved_list_lock);
  // insert to the head of the free list
  spinlock_lock(&free_list_lock);
  if(!free_list) {
    free_list = entry;
    spinlock_unlock(&free_list_lock);
    return;
  }
  entry->prev = NULL;
  entry->next = free_list;
  free_list->prev = entry;
  free_list = entry;
  spinlock_unlock(&free_list_lock);
#endif
}

static void
insert_dw_entry(struct entry_t* entry)
{
  spinlock_lock(&unresolved_list_lock);
  if(!unresolved_list) {
    unresolved_list = entry;
    spinlock_unlock(&unresolved_list_lock);
    return;
  }
  entry->prev = NULL;
  entry->next = unresolved_list;
  unresolved_list->prev = entry;
  unresolved_list = entry;
  spinlock_unlock(&unresolved_list_lock);
}

static struct entry_t*
fetch_dw_entry(struct entry_t **pointer)
{
  spinlock_lock(&unresolved_list_lock);
  if(!(*pointer)) {
    spinlock_unlock(&unresolved_list_lock);
    return NULL;
  }
  while((*pointer) && (*pointer)->flag) (*pointer) = (*pointer)->next;
  if((*pointer)) (*pointer)->flag = true;
  spinlock_unlock(&unresolved_list_lock);
  return (*pointer);
}

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

static void
r_splay_count_update(uint64_t region_id, uint64_t val)
{
  spinlock_lock(&record_tree_lock);
  record_tree_root = r_splay(record_tree_root, region_id);

  if(!record_tree_root){
    spinlock_unlock(&record_tree_lock);
    return;
  }
  if(record_tree_root->region_id != region_id) {
    spinlock_unlock(&record_tree_lock);
    return;
  } else {
    record_tree_root->use_count += val;
    TMSG(DEFER_CTXT, "I am value %d (%d) for region %d", record_tree_root->use_count, val, region_id);
    if((record_tree_root->use_count == 0)) {
      TMSG(DEFER_CTXT, "I am here for delete");
      r_splay_delete(record_tree_root->region_id);
    }
  }
  spinlock_unlock(&record_tree_lock);
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

//
// only master and sub-master thread execute start_team_fn and end_team_fn
//
void start_team_fn(int rank)
{
  hpcrun_async_block();
  // mark the real master thread (the one with process stop)
  if(omp_get_level() == 1 && omp_get_thread_num() == 0) {
    thread_data_t *td = hpcrun_get_thread_data();
    td->master = 1;
  }
  // create new record entry for a new region
  uint64_t region_id = GOMP_get_region_id();
  struct record_t *record = new_record(region_id);
  r_splay_insert(record);
  hpcrun_async_unblock();
}

void end_team_fn()
{
  hpcrun_async_block();
  cct_node_t *node = NULL;
  uint64_t region_id = GOMP_get_region_id();
  struct record_t *record = r_splay_lookup(region_id);
  // insert resolved root to the corresponding record entry
  if(record) {
    if(record->use_count > 0) {
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
        node = hpcrun_sample_callpath(&uc, 0, 0, 2, 1, (void *)&omp_arg);
        // make sure the outer region should be unwound
        uint64_t unresolved_region_id = is_partial_resolve(node);
        if(unresolved_region_id) {
     	  r_splay_count_update(unresolved_region_id, 1L);
        }
      }
      //
      // for master thread in the outer-most region, a normal unwind to the process stop 
      //
      else
        node = hpcrun_sample_callpath(&uc, 0, 0, 2, 1, NULL);

      record->node = node;
    }
    else {
      r_splay_count_update(record->region_id, 0L);
    }
  }
  hpcrun_async_unblock();
}

void register_defer_callback()
{
  GOMP_team_callback_register(start_team_fn, end_team_fn);
}

int need_defer_cntxt()
{
  // master thread does not need to defer the context
  if(ENABLED(SET_DEFER_CTXT) && GOMP_get_region_id() > 0 && !TD_GET(master)) {//omp_get_thread_num() != 0) {
    thread_data_t *td = hpcrun_get_thread_data();
    td->defer_flag = 1;
    return 1;
  }
  return 0;
}

static cct_node_t*
is_resolved(uint64_t id)
{
  struct record_t *record = r_splay_lookup(id);
  if(! record) return NULL;
  return r_splay_lookup(id)->node;
}

//
// TODO: add trace correction info here
//
static void
merge_metrics(cct_node_t *a, cct_node_t *b, merge_op_arg_t arg)
{
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
  thread_data_t *td = NULL;
  if(a) 
    td = (thread_data_t*) a;
  cct_node_t *prefix;
  uint64_t my_region_id = (uint64_t)hpcrun_cct_addr(cct)->ip_norm.lm_ip;
  TMSG(DEFER_CTXT, " try to resolve region %d", my_region_id);
  if (prefix = is_resolved(my_region_id)) {
    // delete cct from its original parent before merging
    hpcrun_cct_delete_self(cct);
    if(!is_partial_resolve(prefix)) {
      if(!td)
        prefix = hpcrun_cct_insert_path(prefix, hpcrun_get_process_stop_cct());
      else
        prefix = hpcrun_cct_insert_path(prefix, (td->epoch->csdata).tree_root);
    }
    else {
      if(!td)
        prefix = hpcrun_cct_insert_path(prefix, hpcrun_get_tbd_cct());
      else
        prefix = hpcrun_cct_insert_path(prefix, (td->epoch->csdata).unresolved_root);
    }
    // adjust the callsite of the prefix in side threads to make sure they are the same as
    // in the master thread. With this operation, all sides threads and the master thread
    // will have the unified view for parallel regions (this only works for GOMP)
    if (DISABLED(KEEP_GOMP_START))
      hpcrun_cct_addr(prefix)->ip_norm.lm_ip -= 5L;
    hpcrun_cct_merge(prefix, cct, merge_metrics, NULL);
    // must delete it when not used considering the performance
    r_splay_count_update(my_region_id, -1L);
  }
}

static void
omp_resolve_and_free(cct_node_t* cct, cct_op_arg_t a, size_t l)
{
  omp_resolve(cct, a, l);
}

void resolve_cntxt()
{
  hpcrun_async_block();
  uint64_t current_region_id = GOMP_get_region_id();
  int current_thread_num = omp_get_thread_num();
  cct_node_t* tbd_cct = hpcrun_get_tbd_cct();
  thread_data_t *td = hpcrun_get_thread_data();
  // resolve the trees at the end of one parallel region
  if((td->region_id != current_region_id) && (td->region_id != 0) && 
     (current_thread_num != 0)) {
    TMSG(DEFER_CTXT, "I want to resolve the context when I come out from region %d", td->region_id);
    hpcrun_cct_walkset(tbd_cct, omp_resolve_and_free, NULL);
  }
  // update the use count when come into a new omp region
  if((td->region_id != current_region_id) && (current_region_id != 0) &&
     (current_thread_num != 0)) {
    hpcrun_cct_insert_addr(tbd_cct, &(ADDR2(UNRESOLVED, current_region_id)));
    r_splay_count_update(current_region_id, 1L);
  }
  // td->region_id represents the out-most parallel region id
  if(current_thread_num != 0)    td->region_id = current_region_id;

  hpcrun_async_unblock();
}

static void
tbd_test(cct_node_t* cct, cct_op_arg_t a, size_t l)
{
  thread_data_t *td = (thread_data_t *)a;
  if(hpcrun_cct_addr(cct)->ip_norm.lm_id == (uint16_t)UNRESOLVED) {
    TMSG(DEFER_CTXT, "I cannot resolve region %d", (uint64_t)hpcrun_cct_addr(cct)->ip_norm.lm_ip);
    td->defer_write = 1;
  }
}

void resolve_other_cntxt(bool fini_flag)
{
  //
  // try to resolve any entry
  // entry is a local pointer
  //
  hpcrun_async_block();
  spinlock_lock(&unresolved_list_lock);
  struct entry_t *pointer = unresolved_list;
  spinlock_unlock(&unresolved_list_lock);
  struct entry_t *entry = NULL;
  while(entry = fetch_dw_entry(&pointer)) {
    hpcrun_cct_walkset((entry->td->epoch->csdata).unresolved_root, omp_resolve_and_free, (cct_op_arg_t)(entry->td));
    entry->td->defer_write = 0;
    // if at stop point, write out what we have (no need to check tbd again)
    if(!fini_flag)
      hpcrun_cct_walkset((entry->td->epoch->csdata).unresolved_root, tbd_test, (cct_op_arg_t)(entry->td));
    if(!entry->td->defer_write) {
      thread_data_t *td = hpcrun_get_thread_data();
      cct2metrics_t* store_cct2metrics_map = td->cct2metrics_map;
      td->cct2metrics_map = entry->td->cct2metrics_map;
      hpcrun_write_other_profile_data(entry->td->epoch, entry->td);
      td->cct2metrics_map = store_cct2metrics_map;
      pointer = pointer->next;
      delete_dw_entry(entry);
    }
    else {
      pointer = pointer->next;
      entry->flag = false;
    }
  }
  hpcrun_async_unblock();
}

void resolve_cntxt_fini()
{
  hpcrun_async_block();
  thread_data_t *td = hpcrun_get_thread_data();
  hpcrun_cct_walkset(hpcrun_get_tbd_cct(), omp_resolve_and_free, NULL);
  hpcrun_cct_walkset(hpcrun_get_tbd_cct(), tbd_test, (cct_op_arg_t)td);
  if(td->defer_write) {
    struct entry_t* entry = new_dw_entry();
    entry->td = td;
    insert_dw_entry(entry);
  }
  hpcrun_async_unblock();
}
