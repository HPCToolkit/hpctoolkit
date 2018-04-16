#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include <cupti_activity.h>
#include "cupti-node.h"

void
cupti_activity_node_set
(
 cupti_node_t *cupti_node,
 CUpti_Activity* activity, 
 cct_node_t *cct_node,
 cct_node_t *next
)
{
  cupti_entry_activity_t *entry = (cupti_entry_activity_t *)(cupti_node->entry);
  entry->activity = activity;
  entry->cct_node = cct_node;
  cupti_node->next = next;
  cupti_node->type = CUPTI_ENTRY_TYPE_ACTIVITY;
//  TODO(Keren): do not copy activities, we need another memory pool for activity buffer
}


cupti_node_t *
cupti_activity_node_new
(
 CUpti_Activity* activity, 
 cct_node_t *cct_node,
 cct_node_t *next
)
{
  cupti_node_t *node = (cupti_node_t *)hpcrun_malloc(sizeof(cupti_node_t));
  cupti_entry_activity_t *entry = (cupti_entry_activity_t *)hpcrun_malloc(sizeof(cupti_entry_activity_t));
  node->entry = entry;
  cupti_activity_node_set(node, activity, cct_node, next);
  return node;
}


void
cupti_notification_node_set
(
 cupti_node_t *cupti_node,
 int64_t host_op_id,
 cct_node_t *cct_node,
 void *record,
 cupti_node_t *next
)
{
  cupti_entry_notification_t *entry = (cupti_entry_notification_t *)(cupti_node->entry);
  entry->host_op_id = host_op_id;
  entry->cct_node = cct_node;
  entry->record = record;
  cupti_node->next = next;
  cupti_node->type = CUPTI_ENTRY_TYPE_NOTIFICATION;
}


cupti_node_t *
cupti_notification_node_new
(
 int64_t host_op_id,
 cct_node_t *cct_node,
 void *record,
 cupti_node_t *next
)
{
  cupti_node_t *node = (cupti_node_t *)hpcrun_malloc(sizeof(cupti_node_t));
  cupti_entry_notification_t *entry = (cupti_entry_notification_t *)hpcrun_malloc(sizeof(cupti_entry_notification_t));
  node->entry = entry;
  cupti_notification_node_set(node, host_op_id, cct_node, record, next);
  return node;
}
