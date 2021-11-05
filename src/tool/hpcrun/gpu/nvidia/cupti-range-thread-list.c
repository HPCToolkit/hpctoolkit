#include "cupti-range-thread-list.h"

#include <hpcrun/memory/hpcrun-malloc.h>
#include <hpcrun/messages/messages.h>
#include <stdio.h>


typedef struct cupti_range_thread {
  struct cupti_range_thread *next;
  int thread_id;
} cupti_range_thread_t;

static cupti_range_thread_t *head = NULL;
static cupti_range_thread_t *cur = NULL;
static cupti_range_thread_t *free_list = NULL;

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
  return thread;
}

void cupti_range_thread_list_add(int thread_id) {
  if (head == NULL) {
    head = cupti_range_thread_list_alloc(thread_id);
    cur = head;
    head->next = head;
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
  }
}

void cupti_range_thread_list_advance_cur() {
  if (cur != NULL) {
    cur = cur->next;
  }
}

bool cupti_range_thread_list_is_cur(int thread_id) {
  if (cur == NULL) {
    return false;
  }
  return cur->thread_id == thread_id;
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
}

int cupti_range_thread_list_num_threads() {
  int num_threads = 0;
  if (head == NULL) {
    return num_threads;
  }

  ++num_threads;
  cupti_range_thread_t *next = head->next;
  while (next != head) {
    ++num_threads;
    next = next->next;
  }

  return num_threads;
}
