#include "cupti-range-thread-list.h"

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/thread_data.h>

#include "../gpu-range.h"

#include "cupti-cct-trie.h"

typedef struct cupti_range_thread_notification {
  uint32_t context_id;
  uint32_t range_id;
  bool active;
  bool logic;
} cupti_range_thread_notification_t;

typedef struct cupti_range_thread {
  struct cupti_range_thread *next;
  int thread_id;
  cupti_range_thread_notification_t notification;
} cupti_range_thread_t;

// Thread list is a circle
// if head != NULL, there's always a pointer->next = head
static cupti_range_thread_t *head = NULL;
static cupti_range_thread_t *free_list = NULL;
static __thread cupti_range_thread_t *cur_thread = NULL;
static int num_threads = 0;

/************************************************
 * Private
 ***********************************************/

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
  cupti_range_thread_list_notification_clear();
  return thread;
}

/************************************************
 * Interface
 ***********************************************/

void cupti_range_thread_list_add() {
  thread_data_t *cur_td = hpcrun_safe_get_td();
  core_profile_trace_data_t *cptd = &cur_td->core_profile_trace_data;
  int thread_id = cptd->id;

  if (head == NULL) {
    head = cupti_range_thread_list_alloc(thread_id);
    head->next = head;
    cur_thread = head;
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
    cur_thread = node;
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


void cupti_range_thread_list_notification_update(void *entry, uint32_t context_id, uint32_t range_id, bool active, bool logic) {
  cupti_range_thread_t *thread = (cupti_range_thread_t *)entry;
  if (thread->notification.range_id != GPU_RANGE_NULL) {
    // Only the first time matters
    return;
  }

  thread->notification.context_id = context_id;
  thread->notification.range_id = range_id;
  thread->notification.active = active;
  thread->notification.logic = logic;
}


void cupti_range_thread_list_notification_clear() {
  if (cur_thread == NULL) {
    return;
  }

  cupti_range_thread_t *thread = cur_thread;
  thread->notification.context_id = 0;
  thread->notification.range_id = GPU_RANGE_NULL;
  thread->notification.active = false;
  thread->notification.logic = false;
}


int cupti_range_thread_list_num_threads() {
  return num_threads;
}


int cupti_range_thread_list_id_get() {
  if (cur_thread != NULL) {
    return cur_thread->thread_id;
  } else {
    return CUPTI_RANGE_THREAD_NULL;
  }
}


uint32_t cupti_range_thread_list_notification_context_id_get() {
  if (cur_thread != NULL) {
    return cur_thread->notification.context_id;
  } else {
    return 0;
  }
}


uint32_t cupti_range_thread_list_notification_range_id_get() {
  if (cur_thread != NULL) {
    return cur_thread->notification.range_id;
  } else {
    return GPU_RANGE_NULL;
  }
}


bool cupti_range_thread_list_notification_active_get() {
  if (cur_thread != NULL) {
    return cur_thread->notification.active;
  } else {
    return false;
  }
}


bool cupti_range_thread_list_notification_logic_get() {
  if (cur_thread != NULL) {
    return cur_thread->notification.logic;
  } else {
    return false;
  }
}


void cupti_range_thread_list_apply(cupti_range_thread_list_fn_t fn, void *args) {
  if (head == NULL) {
    return;
  }

  cupti_range_thread_t *next = head->next;
  while (next != head) {
    fn(next->thread_id, (void *)next, args);
    next = next->next;
  }
  fn(next->thread_id, (void *)next, args);
}
