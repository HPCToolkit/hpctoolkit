#ifndef cupti_range_thread_list_h
#define cupti_range_thread_list_h

#include <stdbool.h>

#include <hpcrun/thread_data.h>

#include "cupti-cct-trie.h"

#define CUPTI_RANGE_THREAD_NULL -1

typedef void (*cupti_range_thread_list_fn_t)
(
 int thread_id,
 void *entry,
 void *args
);

void cupti_range_thread_list_add();

void cupti_range_thread_list_apply(cupti_range_thread_list_fn_t fn, void *args);

void cupti_range_thread_list_notification_update(void *entry, uint32_t context_id, uint32_t range_id, bool active, bool logic);

void cupti_range_thread_list_notification_clear();

int cupti_range_thread_list_num_threads();

int cupti_range_thread_list_id_get();

uint32_t cupti_range_thread_list_notification_context_id_get();

uint32_t cupti_range_thread_list_notification_range_id_get();

bool cupti_range_thread_list_notification_active_get();

bool cupti_range_thread_list_notification_logic_get();

void cupti_range_thread_list_clear();

#endif 
