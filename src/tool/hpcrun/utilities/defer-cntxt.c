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

#include <utilities/defer-cntxt.h>
#include <hpcrun/unresolved.h>

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
  struct record_t *record;
  spinlock_lock(&record_tree_lock);
  record_tree_root = r_splay(record_tree_root, id);
  if(record_tree_root->region_id != id) {
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
printf("I am value %d (%d) for region %d\n", record_tree_root->use_count, val, region_id);
    if((record_tree_root->use_count == 0)) {
printf("I am hererererere for delete\n");fflush(stdout);
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

void start_team_fn(int rank)
{
  hpcrun_async_block();
  // create new record entry for a new region
  uint64_t region_id = GOMP_get_region_id();
  assert(region_id >0);
  struct record_t *record = new_record(region_id);
  r_splay_insert(record);
  hpcrun_async_unblock();
}

void end_team_fn()
{
  uint64_t region_id = GOMP_get_region_id();
  struct record_t *record = r_splay_lookup(region_id);
  printf("region is %d\n", region_id); fflush(stdout);
  assert(record);
  // insert resolved root to the corresponding record entry
  if(record->use_count > 0) {
    hpcrun_async_block();
    ucontext_t uc;
    getcontext(&uc);
    cct_node_t *node = hpcrun_sample_callpath(&uc, 0, 0, 0, 1, NULL);
    node = hpcrun_cct_parent(node);
    record->node = node;
    assert(record->node);
    hpcrun_async_unblock();
  }
}

void register_callback()
{
  GOMP_team_callback_register(start_team_fn, end_team_fn);
}

int need_defer_cntxt()
{
  // master thread does not need to defer the context
  if(GOMP_get_region_id() > 0 && omp_get_thread_num() != 0) {
    thread_data_t *td = hpcrun_get_thread_data();
    td->defer_flag = 1;
    return 1;
  }
  return 0;
}

static cct_node_t*
is_resolved(uint64_t id)
{
  return r_splay_lookup(id)->node;
}


#if 0
static void
omp_resolve(cct_node_t* cct, walk_arg_t arg)
{
  cct_node_t *prefix;
  if (prefix = is_resolved(my_region_id)) {
    full_tree = cct_join_prefix(prefix, unique_child_cct(cct));
    cct_merge(cct_bundle_processs_stop, full_tree);
    // put remaining cct nodes back on free list
    r_splay_count_update(td->region_id, -1L);
  }
}
#endif

void resolve_cntxt()
{
  thread_data_t *td = hpcrun_get_thread_data();
  // resolve the trees at the end of one parallel region
  if((td->region_id != GOMP_get_region_id()) && (td->region_id != 0) && 
     (omp_get_thread_num() != 0)) {
    printf("I want to resolve the context for region %d\n", td->region_id);
//    foreach_child(hpcrun_get_tbd_cct(), omp_resolve, NULL);
  }
  // update the use count when come into a new omp region
  if((td->region_id != GOMP_get_region_id()) && (GOMP_get_region_id() != 0) &&
     (omp_get_thread_num() != 0)) {
    hpcrun_cct_insert_addr(hpcrun_get_tbd_cct(), &(ADDR2(UNRESOLVED, GOMP_get_region_id())));
    r_splay_count_update(GOMP_get_region_id(), 1L);
  }
  td->region_id = GOMP_get_region_id();
}

void resolve_cntxt_fini()
{
  thread_data_t *td = hpcrun_get_thread_data();
  if(td->region_id > 0) {
    printf("fini, resolve for region %d\n", td->region_id);
//    foreach_child(hpcrun_get_tbd_cct(), omp_resolve, NULL);
  }
}
