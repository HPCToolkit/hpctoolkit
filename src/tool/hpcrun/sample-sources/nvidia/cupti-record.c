#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_version.h>
#include "cupti-stack.h"
#include "cupti-record.h"

static _Atomic(cupti_record_list_t *) cupti_record_list_head;

static __thread cupti_record_t cupti_record = 
{ 
  .worker_notification_stack = { .head = {NULL}}, 
  .worker_free_notification_stack = { .head = {NULL}}, 
  .worker_activity_stack = { .head = {NULL}}, 
  .worker_free_activity_stack = { .head = {NULL}}, 
  .cupti_notification_stack = { .head = {NULL}}, 
  .cupti_free_notification_stack = { .head = {NULL}}, 
  .cupti_activity_stack = { .head = {NULL}}, 
  .cupti_free_activity_stack = { .head = {NULL}},
  .cupti_buffer_stack = { .head = {NULL}}
};

static __thread bool cupti_record_initialized = false;


void
cupti_record_init()
{
  if (!cupti_record_initialized) {
    cupti_stack_init(&cupti_record.worker_notification_stack);
    cupti_stack_init(&cupti_record.worker_free_notification_stack);
    cupti_stack_init(&cupti_record.worker_activity_stack);
    cupti_stack_init(&cupti_record.worker_free_activity_stack);
    cupti_stack_init(&cupti_record.cupti_notification_stack);
    cupti_stack_init(&cupti_record.cupti_free_notification_stack);
    cupti_stack_init(&cupti_record.cupti_activity_stack);
    cupti_stack_init(&cupti_record.cupti_free_activity_stack);
    cupti_record_initialized = true;

    cupti_record_list_t *curr_cupti_record_list_head = hpcrun_malloc_safe(sizeof(cupti_record_list_t));
    cupti_record_list_t *old_head = atomic_load(&cupti_record_list_head);
    atomic_store(&curr_cupti_record_list_head->next, old_head);
    curr_cupti_record_list_head->record = &cupti_record;
    while (!atomic_compare_exchange_strong(
      &cupti_record_list_head, &(curr_cupti_record_list_head->next), curr_cupti_record_list_head));
  }
}


cupti_record_t *
cupti_record_get()
{
  cupti_record_init();
  return &cupti_record;
}


void
cupti_worker_notification_apply(uint64_t host_op_id, cct_node_t *cct_node)
{
  cupti_stack_t *worker_notification_stack = &(cupti_record.worker_notification_stack);
  cupti_stack_t *worker_free_notification_stack = &(cupti_record.worker_free_notification_stack);
  cupti_node_t *node = cupti_stack_pop(worker_free_notification_stack);
  if (node == NULL) {
    cupti_stack_t *cupti_free_notification_stack = &(cupti_record.cupti_free_notification_stack);
    cupti_stack_steal(worker_free_notification_stack, cupti_free_notification_stack);
    node = cupti_stack_pop(worker_free_notification_stack);
  }
  if (node == NULL) {
    node = cupti_notification_node_new(host_op_id, cct_node, &cupti_record, NULL);
  } else {
    cupti_notification_node_set(node, host_op_id, cct_node, &cupti_record, NULL);
  } 
  cupti_stack_push(worker_notification_stack, node);
}


void
cupti_worker_activity_apply(cupti_stack_fn_t fn)
{
  cupti_stack_t *worker_activity_stack = &(cupti_record.worker_activity_stack);
  cupti_stack_t *cupti_activity_stack = &(cupti_record.cupti_activity_stack);
  cupti_stack_steal(worker_activity_stack, cupti_activity_stack);
  if (cupti_stack_head(worker_activity_stack) != NULL) {
    cupti_stack_t *worker_free_activity_stack = &(cupti_record.worker_free_activity_stack);
    cupti_stack_apply(worker_activity_stack, worker_free_activity_stack, fn);
  }
}


void
cupti_cupti_notification_apply(cupti_stack_fn_t fn)
{
  cupti_record_list_t *head = atomic_load(&cupti_record_list_head);
  while (head != NULL) {
    cupti_stack_t *worker_notification_stack = &(head->record->worker_notification_stack);
    cupti_stack_t *cupti_notification_stack = &(head->record->cupti_notification_stack);
    cupti_stack_steal(cupti_notification_stack, worker_notification_stack);
    if (cupti_stack_head(cupti_notification_stack) != NULL) {
      cupti_stack_t *cupti_free_notification_stack = &(head->record->cupti_free_notification_stack);
      cupti_stack_apply(cupti_notification_stack, cupti_free_notification_stack, fn);
    }
    head = atomic_load(&head->next);
  } 
}


void
cupti_cupti_activity_apply(CUpti_Activity *activity, cct_node_t *cct_node, cupti_record_t *record)
{
  cupti_stack_t *cupti_activity_stack = &(record->cupti_activity_stack);
  cupti_stack_t *cupti_free_activity_stack = &(record->cupti_free_activity_stack);
  cupti_node_t *node = cupti_stack_pop(cupti_free_activity_stack);
  if (node == NULL) {
    cupti_stack_t *worker_free_activity_stack = &(record->worker_free_activity_stack);
    cupti_stack_steal(cupti_free_activity_stack, worker_free_activity_stack);
    node = cupti_stack_pop(cupti_free_activity_stack);
  }
  if (node == NULL) {
    node = cupti_activity_node_new(activity, cct_node, NULL);
  } else {
    cupti_activity_node_set(node, activity, cct_node, NULL);
  } 
  cupti_stack_push(cupti_activity_stack, node);
}
