#include "cupti-range-thread-list.h"

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/thread_data.h>

#include "cupti-cct-trie.h"

typedef struct cupti_range_thread {
  struct cupti_range_thread *next;
  int thread_id;
  cupti_cct_trie_node_t *trie_root;
  cupti_cct_trie_node_t **trie_cur_ptr;
  cupti_cct_trie_node_t **trie_logic_root_ptr;
} cupti_range_thread_t;

// Thread list is a circle
// if head != NULL, there's always a pointer->next = head
static cupti_range_thread_t *head = NULL;
static cupti_range_thread_t *free_list = NULL;
static int num_threads = 0;

static cupti_range_thread_t *free_list_alloc() {
  cupti_range_thread_t *thread = NULL;
  if (free_list) {
    thread = free_list;
    free_list = free_list->next;
  } else {
    thread = (cupti_range_thread_t *)hpcrun_malloc_safe(sizeof(cupti_range_thread_t));
  }
  thread->next = NULL;
  return thread;
}


static void free_list_free(cupti_range_thread_t *thread) {
  thread->next = free_list;
  free_list = thread;
}


static cupti_range_thread_t *cupti_range_thread_list_alloc(int thread_id) {
  cupti_range_thread_t *thread = free_list_alloc();
  thread->thread_id = thread_id;
  thread->trie_root = cupti_cct_trie_root_get();
  thread->trie_logic_root_ptr = cupti_cct_trie_logic_root_ptr_get();
  thread->trie_cur_ptr = cupti_cct_trie_cur_ptr_get();
  return thread;
}


void cupti_range_thread_list_add() {
  thread_data_t *cur_td = hpcrun_safe_get_td();
  core_profile_trace_data_t *cptd = &cur_td->core_profile_trace_data;
  int thread_id = cptd->id;

  if (head == NULL) {
    head = cupti_range_thread_list_alloc(thread_id);
    head->next = head;
    ++num_threads;
  } else {
    cupti_range_thread_t *next = head->next;
    cupti_range_thread_t *prev = head;
    while (next != head) {
      if (next->thread_id == thread_id) {
        // Duplicate id
        return;
      }
      prev = next;
      next = next->next;
    }
    if (next->thread_id == thread_id) {
      // Single node
      return;
    }
    cupti_range_thread_t *node = cupti_range_thread_list_alloc(thread_id);
    prev->next = node;
    node->next = next;
    ++num_threads;
  }
}


void cupti_range_thread_list_clear() {
  if (head == NULL) {
    return;
  }

  cupti_range_thread_t *next = head->next;
  cupti_range_thread_t *prev = head;
  while (next != head) {
    prev = next;
    next = next->next;
    free_list_free(prev);
  }
  free_list_free(head);
  head = NULL;
  num_threads = 0;
}


int cupti_range_thread_list_num_threads() {
  return num_threads;
}


void cupti_range_thread_list_apply(cupti_range_thread_list_fn_t fn, void *args) {
  if (head == NULL) {
    return;
  }

  cupti_range_thread_t *next = head->next;
  while (next != head) {
    fn(next->thread_id, next->trie_root, next->trie_logic_root_ptr, next->trie_cur_ptr, args);
    next = next->next;
  }
  fn(next->thread_id, next->trie_root, next->trie_logic_root_ptr, next->trie_cur_ptr, args);
}
