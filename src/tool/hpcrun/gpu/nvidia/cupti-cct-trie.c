#include "cupti-cct-trie.h"

#include <stdio.h>
#include <zlib.h>

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/safe-sampling.h>
#include <lib/prof-lean/splay-macros.h>

#include "cupti-range.h"
#include "cupti-range-thread-list.h"
#include "../gpu-metrics.h"
#include "../gpu-range.h"
#include "../gpu-op-placeholders.h"

//#define ENABLE_LZ


typedef struct cupti_cct_trie_trace_s {
  cct_node_t **keys;
  uint32_t *range_ids;

#ifdef ENABLE_LZ
  uint8_t *keys_buf;
  uint8_t *range_ids_buf;
  size_t keys_buf_size;
  size_t range_ids_buf_size;
#endif
  size_t size; // Number of keys and ranges in this trace
} cupti_cct_trace_node_t;

struct cupti_cct_trie_node_s {
  cct_node_t *key;
  uint32_t range_id;

  struct cupti_cct_trie_node_s* children;
  struct cupti_cct_trie_node_s* parent;

  struct cupti_cct_trie_node_s* left;
  struct cupti_cct_trie_node_s* right;

  cupti_cct_trace_node_t* trace;
};

typedef struct cupti_cct_trie_ptr_s {
  cupti_cct_trie_node_t *node;
  size_t ptr;
} cupti_cct_trie_ptr_t;

static __thread cupti_cct_trie_node_t *trie_root = NULL;
static __thread cupti_cct_trie_node_t *free_list = NULL;
static __thread cupti_cct_trie_ptr_t trie_logic_root = {
  .node = NULL,
  .ptr = CUPTI_CCT_TRIE_PTR_NULL
};
static __thread cupti_cct_trie_ptr_t trie_cur = {
  .node = NULL,
  .ptr = CUPTI_CCT_TRIE_PTR_NULL
};
static __thread uint32_t single_path = 0;

//***********************************
// splay
//***********************************
static bool cct_trie_cmp_lt(cct_node_t *left, cct_node_t *right) {
  return left < right;
}

static bool cct_trie_cmp_gt(cct_node_t *left, cct_node_t *right) {
  return left > right;
}

#define l_lt(a, b) cct_trie_cmp_lt(a, b)
#define l_gt(a, b) cct_trie_cmp_gt(a, b)

static cupti_cct_trie_node_t *
splay(cupti_cct_trie_node_t *node, cct_node_t *key)
{
  GENERAL_SPLAY_TREE(cupti_cct_trie_node_s, node, key, key, key, left, right, l_lt, l_gt);
  return node;
}

//***********************************
// allocator
//***********************************

static cupti_cct_trie_node_t *
cct_trie_alloc_helper
(
 cupti_cct_trie_node_t **free_list, 
 size_t size
)
{
  cupti_cct_trie_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->left;
  } else {
    first = (cupti_cct_trie_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


static void
cct_trie_free_helper
(
 cupti_cct_trie_node_t **free_list, 
 cupti_cct_trie_node_t *e 
)
{
  e->left = *free_list;
  *free_list = e;
}

#define trie_alloc(free_list)    \
  (cupti_cct_trie_node_t *) cct_trie_alloc_helper    \
  ((cupti_cct_trie_node_t **) free_list, sizeof(cupti_cct_trie_node_t))  

#define trie_free(free_list, node)      \
  cct_trie_free_helper          \
  ((cupti_cct_trie_node_t **) free_list,        \
   (cupti_cct_trie_node_t *) node)


static void
cct_trace_free
(
 cupti_cct_trie_node_t *node
)
{
  if (node->trace == NULL) {
    return;
  }

  hpcrun_safe_enter();

  if (node->trace->keys != NULL) {
    free(node->trace->keys);
  }

  if (node->trace->range_ids != NULL) {
    free(node->trace->range_ids);
  }

#ifdef ENABLE_LZ
  if (node->trace->keys_buf != NULL) {
    free(node->trace->keys_buf);
    node->trace->keys_buf_size = 0;
  }

  if (node->trace->range_ids_buf != NULL) {
    free(node->trace->range_ids_buf);
    node->trace->range_ids_buf_size = 0;
  }
#endif

  free(node->trace);

  hpcrun_safe_exit();

  node->trace = NULL;
}


static cupti_cct_trace_node_t *
cct_trace_new
(
 size_t size
)
{
  hpcrun_safe_enter();

  cupti_cct_trace_node_t *trace = (cupti_cct_trace_node_t *)malloc(sizeof(cupti_cct_trace_node_t));
  trace->keys = (cct_node_t **)malloc((size + 1) * sizeof(cct_node_t *));
  trace->range_ids = (uint32_t *)malloc((size + 1) * sizeof(uint32_t));
  trace->size = size;

  hpcrun_safe_exit();

#ifdef ENABLE_LZ
  trace->keys_buf = NULL;
  trace->keys_buf_size = 0;
  trace->range_ids_buf = NULL;
  trace->range_ids_buf_size = 0;
#endif

  return trace;
}


static cct_node_t *
cct_trace_get_key
(
 cupti_cct_trace_node_t *trace,
 size_t ptr
)
{
  return trace->keys[ptr];
}


static uint32_t
cct_trace_get_range_id
(
 cupti_cct_trace_node_t *trace,
 size_t ptr
)
{
  return trace->range_ids[ptr];
}


static cupti_cct_trie_node_t *
cct_trie_new
(
 cupti_cct_trie_node_t *parent,
 cct_node_t *key,
 uint32_t range_id
)
{
  cupti_cct_trie_node_t *trie_node = trie_alloc(&free_list);
  trie_node->range_id = range_id;
  trie_node->key = key;
  trie_node->parent = parent;
  trie_node->children = NULL;
  trie_node->left = NULL;
  trie_node->right = NULL;
  trie_node->trace = NULL;

  return trie_node;
}


static void
cct_trie_reset
(
)
{
  trie_logic_root.node = trie_root;
  trie_logic_root.ptr = CUPTI_CCT_TRIE_PTR_NULL;
  trie_cur.node = trie_root;
  trie_cur.ptr = CUPTI_CCT_TRIE_PTR_NULL;
  single_path = 0;
}


static void
cct_trie_init
(
)
{
  if (trie_root == NULL) {
    trie_root = cct_trie_new(NULL, NULL, GPU_RANGE_NULL);
    cct_trie_reset();
  }
}


#ifdef ENABLE_LZ
static void
cct_trie_trace_lz_compress
(
 uint8_t *input,
 uint8_t **output,
 size_t input_size,
 size_t *output_size
)
{
  *output_size = input_size;

  hpcrun_safe_enter();
  *output = (uint8_t *)malloc(*output_size);
  hpcrun_safe_exit();

  // keys
  z_stream defstream;
  defstream.zalloc = Z_NULL;
  defstream.zfree = Z_NULL;
  defstream.opaque = Z_NULL;
  // Input bytes
  defstream.avail_in = input_size;
  // Input pointer
  defstream.next_in = input;
  // Output size
  defstream.avail_out = *output_size;
  // Output pointer
  defstream.next_out = *output;

  deflateInit(&defstream, Z_BEST_COMPRESSION);
  deflate(&defstream, Z_FINISH);
  deflateEnd(&defstream);

  size_t left_size = *output_size - defstream.avail_out;

  // Debug compress rate
  printf("Compress origin size %zu, current size %zu\n", *output_size, left_size);
  *output = (uint8_t *)realloc(*output, left_size + 1);
  *output_size = left_size + 1;
}


static void
cct_trie_trace_lz_decompress
(
 uint8_t *input,
 uint8_t **output,
 size_t input_size,
 size_t output_size
)
{
  hpcrun_safe_enter();
  *output = (uint8_t *)malloc(output_size);
  hpcrun_safe_exit();

  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  infstream.avail_in = input_size;
  infstream.next_in = input;
  infstream.avail_out = output_size;
  infstream.next_out = *output;

  inflateInit(&infstream);
  inflate(&infstream, Z_NO_FLUSH);
  inflateEnd(&infstream);

  size_t left_size = output_size - infstream.avail_out;

  // Debug compress rate
  printf("Decompress origin size %zu, current size %zu\n", input_size, left_size);
}


static void
cct_trie_trace_lz_release
(
 cupti_cct_trace_node_t *trace
)
{
  hpcrun_safe_enter();
  if (trace->keys != NULL && trace->keys_buf != NULL && trace->keys_buf_size != 0) {
    free(trace->keys);
  }  
  if (trace->range_ids != NULL && trace->range_ids_buf != NULL && trace->range_ids_buf_size != 0) {
    free(trace->range_ids);
  }
  hpcrun_safe_exit();

  trace->keys = NULL;
  trace->range_ids = NULL;
}

#endif



static void
cct_trie_trace_split()
{
  cupti_cct_trace_node_t *trace_cur = trie_cur.node->trace;
  cupti_cct_trie_node_t *node_cur = trie_cur.node;
  size_t ptr_cur = trie_cur.ptr;

  // Split from the middle
  size_t prev_size = ptr_cur == 0 ? 0 : ptr_cur - 1;
  size_t next_size = ptr_cur == trace_cur->size - 1 ? 0 : trace_cur->size - prev_size - 1;

  cupti_cct_trie_node_t *parent = node_cur;
  cupti_cct_trie_node_t *children = node_cur->children;
  size_t i;
  for (i = 0; i < prev_size; ++i) {
    // node_cur<->next1<->next2
    cupti_cct_trie_node_t *next = cct_trie_new(parent, cct_trace_get_key(trace_cur, i),
      cct_trace_get_range_id(trace_cur, i));
    parent->children = next;
    parent = next;
  }

  cupti_cct_trie_node_t *middle = cct_trie_new(parent, cct_trace_get_key(trace_cur, prev_size),
    cct_trace_get_range_id(trace_cur, prev_size));
  parent->children = middle;
  parent = middle;

  for (i = prev_size + 1; i < next_size; ++i) {
    cupti_cct_trie_node_t *next = cct_trie_new(parent, cct_trace_get_key(trace_cur, i),
      cct_trace_get_range_id(trace_cur, i));
    parent->children = next;
    parent = next;
  }
  parent->children = children;
  if (children != NULL) {
    children->parent = parent;
  }
  
  // Release trace
  cct_trace_free(node_cur);

  // Update cur pointer
  trie_cur.node = middle->parent;
  trie_cur.ptr = CUPTI_CCT_TRIE_PTR_NULL;
}


static bool
cct_trie_trace_append
(
 uint32_t range_id,
 cct_node_t *cct
)
{
  cupti_cct_trace_node_t *trace_cur = trie_cur.node->trace;

  // XXX(Keren): We don't merge two compressed traces
  single_path = 0;

  if (trie_cur.ptr == trace_cur->size) {
    // Move to the children
#ifdef ENABLE_LZ
    cct_trie_trace_lz_release(trace_cur);
#endif
    return false;
  } else if (trie_cur.ptr == CUPTI_CCT_TRIE_PTR_NULL) {
    // Start looking into the trace
#ifdef ENABLE_LZ
    cct_trie_trace_lz_decompress((uint8_t *)trace_cur->keys_buf, (uint8_t **)&(trace_cur->keys),
      trace_cur->keys_buf_size, trace_cur->size * sizeof(cct_node_t *));
    cct_trie_trace_lz_decompress((uint8_t *)trace_cur->range_ids_buf, (uint8_t **)&(trace_cur->range_ids),
      trace_cur->range_ids_buf_size, trace_cur->size * sizeof(uint32_t));
#endif
    trie_cur.ptr = 0;
  }
  
  if (cct_trace_get_key(trace_cur, trie_cur.ptr) == cct) {
    ++trie_cur.ptr;
    return true;
  } else {
    // Split and reinsert
    cct_trie_trace_split();
    return true;
  }
}


static cupti_cct_trie_ptr_t
cct_trie_ptr_parent
(
 cupti_cct_trie_ptr_t cur
)
{
  cur.node = cur.node->parent;
  cur.ptr = cur.node->trace == NULL ? CUPTI_CCT_TRIE_PTR_NULL : cur.node->trace->size;

  return cur;
}


static void
cct_trie_merge_thread
(
 uint32_t context_id,
 uint32_t range_id,
 bool active,
 bool logic
)
{
  cupti_cct_trie_ptr_t cur = trie_cur;
  ip_normalized_t kernel_ip = gpu_op_placeholder_ip(gpu_placeholder_type_kernel);

  while (cur.node != trie_logic_root.node || cur.ptr != trie_logic_root.ptr) {
    cct_node_t *key = NULL;
    if (cur.node->trace == NULL) {
      key = cur.node->key;
      // Go to its parent
      cur = cct_trie_ptr_parent(cur);
    } else {
      if (cur.ptr == 0) {
        // Go to its parent
        key = cur.node->key;
        cur = cct_trie_ptr_parent(cur);
      } else {
        // Go it its prev trace
        --cur.ptr;
        key = cct_trace_get_key(cur.node->trace, cur.ptr);
      }
    }

    cct_node_t *kernel_ph = hpcrun_cct_insert_ip_norm(key, kernel_ip, false);
    cct_node_t *kernel_ph_children = hpcrun_cct_children(kernel_ph);
    cct_node_t *context_node = hpcrun_cct_insert_context(kernel_ph_children, context_id);

    cct_node_t *prev_range_node = hpcrun_cct_insert_range(context_node, range_id);
    uint64_t sampled_kernel_count = active ? 1 : 0;
    gpu_metrics_attribute_kernel_count(prev_range_node, sampled_kernel_count, 1);
  }

  if (logic) {
    trie_logic_root = trie_cur;
  } else {
    cct_trie_reset();
  }
}


static void
cct_trie_single_path_increase
(
)
{
  if (trie_cur.node->left == NULL && trie_cur.node->right == NULL) {
    // Accumulate single path length
    ++single_path;
  } else {
    // Reset cumulative single path length
    single_path = 0;
  }

  if (single_path >= CUPTI_CCT_TRIE_COMPRESS_THRESHOLD) {
    cupti_cct_trie_compress();
    // After compression, always reset single_path
    single_path = 0;
  }
}

//***********************************
// interface
//***********************************

bool
cupti_cct_trie_append
(
 uint32_t range_id,
 cct_node_t *cct
)
{
  cct_trie_init();

  if (trie_cur.node->trace != NULL) {
    if (cct_trie_trace_append(range_id, cct)) {
      return true;
    }
  }

  bool ret;
  cupti_cct_trie_node_t *found = splay(trie_cur.node->children, cct);
  trie_cur.node->children = found;

  if (found && found->key == cct) {
    trie_cur.node = found;
    trie_cur.ptr = CUPTI_CCT_TRIE_PTR_NULL;
    ret = true;
  } else { 
    // Only assign range_id when a node is created
    cupti_cct_trie_node_t *new = cct_trie_new(trie_cur.node, cct, range_id);
    trie_cur.node->children = new;
    trie_cur.node = new;
    trie_cur.ptr = CUPTI_CCT_TRIE_PTR_NULL;

    if (!found) {
      // Do nothing
    } else if (cct < found->key) {
      new->left = found->left;
      new->right = found;
      found->left = NULL;
    } else {
      new->left = found;
      new->right = found->right;
      found->right = NULL;
    }
    ret = false;
  }

  cct_trie_single_path_increase();

  return ret;
}


void
cupti_cct_trie_unwind
(
)
{
  if (trie_cur.node != trie_root) {
    if (trie_cur.node->trace != NULL && trie_cur.ptr != 0) {
      --trie_cur.ptr;
    } else {
      trie_cur = cct_trie_ptr_parent(trie_cur);
    }
  }
}


// Compress is just proof of concept for now
void
cupti_cct_trie_compress
(
)
{
  // A->B->C->D
  // We are currently at D, merge B->C->D to A
  cupti_cct_trace_node_t *trace = cct_trace_new(CUPTI_CCT_TRIE_COMPRESS_THRESHOLD - 1);

  cupti_cct_trie_node_t *cur = trie_cur.node;
  int i;
  for (i = 0; i < CUPTI_CCT_TRIE_COMPRESS_THRESHOLD - 1; ++i) {
    trace->keys[CUPTI_CCT_TRIE_COMPRESS_THRESHOLD - i - 2] = cur->key;
    trace->range_ids[CUPTI_CCT_TRIE_COMPRESS_THRESHOLD - i - 2] = cur->range_id;
    cupti_cct_trie_node_t *parent = cur->parent;
    trie_free(&free_list, cur); 
    cur = parent;
  }

  cur->trace = trace;
  cur->children = trie_cur.node;
  trie_cur.node->parent = cur;
  trie_cur.ptr = CUPTI_CCT_TRIE_PTR_NULL;

#ifdef ENABLE_LZ
  cct_trie_trace_lz_compress((uint8_t *)trace->range_ids, (uint8_t **)&(trace->range_ids_buf),
    sizeof(uint32_t) * trace->size, &trace->range_ids_buf_size);
  cct_trie_trace_lz_compress((uint8_t *)trace->keys, (uint8_t **)&(trace->keys_buf),
    sizeof(cct_node_t *) * trace->size, &trace->keys_buf_size);

  hpcrun_safe_enter();
  free(trace->range_ids);
  free(trace->keys);
  hpcrun_safe_exit();
#endif
}



typedef struct cct_trie_args_s {
  uint32_t context_id;
  uint32_t range_id;
  bool active;
  bool logic;
  bool progress;
} cct_trie_args_t;


void
cct_trie_notify_fn
(
 int thread_id,
 void *entry,
 void *args
)
{
  if (thread_id == cupti_range_thread_list_id_get()) {
    // We don't notify the starting thread
    return;
  }

  cct_trie_args_t *cct_trie_args = (cct_trie_args_t *)args;
  uint32_t context_id = cct_trie_args->context_id;
  uint32_t range_id = cct_trie_args->range_id;
  bool active = cct_trie_args->active;
  bool logic = cct_trie_args->logic;

  cupti_range_thread_list_notification_update(entry, context_id, range_id, active, logic);
}


uint32_t
cupti_cct_trie_flush
(
 uint32_t context_id,
 bool active,
 bool logic
)
{
  cct_trie_init();

  uint32_t prev_range_id = GPU_RANGE_NULL;
 
  if (trie_cur.node->trace == NULL) {
    prev_range_id = trie_cur.node->range_id;
  } else {
    prev_range_id = cct_trace_get_range_id(trie_cur.node->trace, trie_cur.ptr - 1);
  }

  // Traverse up and use original range_id to merge
  cct_trie_merge_thread(context_id, prev_range_id, active, logic);
    
  // Notify the other threads
  cct_trie_args_t args = {
    .context_id = context_id,
    .range_id = prev_range_id,
    .active = active,
    .logic = true
  }; 
  cupti_range_thread_list_apply(cct_trie_notify_fn, &args);

  return prev_range_id;
}


void
cupti_cct_trie_notification_process
(
)
{
  cct_trie_init();

  uint32_t context_id = cupti_range_thread_list_notification_context_id_get();
  uint32_t range_id = cupti_range_thread_list_notification_range_id_get();
  bool active = cupti_range_thread_list_notification_active_get();
  bool logic = cupti_range_thread_list_notification_logic_get();

  if ((trie_cur.node == trie_logic_root.node && trie_cur.ptr == trie_logic_root.ptr) ||
    range_id == GPU_RANGE_NULL) {
    return;
  }

  // Traverse up and use original range_id to merge
  cct_trie_merge_thread(context_id, range_id, active, logic);
  cupti_range_thread_list_notification_clear();
}


cupti_cct_trie_node_t *
cupti_cct_trie_root_get
(
)
{
  cct_trie_init();
  return trie_root;
}


typedef void (*cct_trie_walk_fn)(cupti_cct_trie_node_t* trie_node);

static void
cct_trie_walk_child(cupti_cct_trie_node_t* trie_node, cct_trie_walk_fn cct_trie_walk)
{
  if (!trie_node) return;

  cct_trie_walk_child(trie_node->left, cct_trie_walk);
  cct_trie_walk_child(trie_node->right, cct_trie_walk);
  cct_trie_walk(trie_node);
}


static void
cct_trie_walk_cleanup(cupti_cct_trie_node_t* trie_node)
{
  cct_trie_walk_child(trie_node->children, cct_trie_walk_cleanup);
  trie_free(&free_list, trie_node);
}


void
cupti_cct_trie_cleanup
(
)
{
  cct_trie_walk_cleanup(trie_root);
  cct_trie_reset();
}


//**********************************************
// Debuging
//**********************************************

static __thread int num_nodes = 0;
static __thread int bytes = 0;
static __thread int single_path_nodes = 0;

static void
cct_trie_walk_dump(cupti_cct_trie_node_t* trie_node)
{
  if (!trie_node) return;
  ++num_nodes;
  if (trie_node->children != NULL && trie_node->children->left == NULL && trie_node->children->right == NULL) {
    ++single_path_nodes;
  }
  bytes += sizeof(trie_node);
  printf("key %p, range_id %d\n", trie_node->key, trie_node->range_id);
  if (trie_node->trace != NULL) {
    printf("\ttrace\n");
    int i;
    for (i = 0; i < trie_node->trace->size; ++i) {
      printf("\t\tkey %p, range_id %d\n", cct_trace_get_key(trie_node->trace, i),
        cct_trace_get_range_id(trie_node->trace, i));
    }
#ifdef ENABLE_LZ
    if (trie_node->trace->keys_buf != NULL) {
      bytes += trie_node->trace->keys_buf_size;
    }
    if (trie_node->trace->range_ids_buf != NULL) {
      bytes += trie_node->trace->range_ids_buf_size;
    }
#else
    bytes += trie_node->trace->size * (sizeof(uint32_t) + sizeof(cct_node_t *));
#endif
  }
  cct_trie_walk_child(trie_node->children, cct_trie_walk_dump);
}


void
cupti_cct_trie_dump
(
)
{
  num_nodes = 0;
  bytes = 0;
  single_path_nodes = 0;
  cct_trie_dump(trie_root);
  printf("num_nodes %d, single_path_nodes %d, bytes %d\n", num_nodes, single_path_nodes, bytes);
}


uint32_t
cupti_cct_trie_single_path_length_get
(
)
{
  return single_path;
}
