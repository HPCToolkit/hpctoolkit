//
// Created by dejan on 7/15/20.
//

#include "gpu-monitors.h"
#include "hpcrun-malloc.h"


static __thread gpu_monitor_node_t *gpu_monitor_list = NULL;

void
gpu_monitor_register(	gpu_monitor_node_t node)
{
  gpu_monitor_node_t* new_node = hpcrun_malloc(sizeof(gpu_monitor_node_t));
  new_node->component = node.component;
  new_node->enter_fn = node.enter_fn;
  new_node->exit_fn = node.exit_fn;
  new_node->next = gpu_monitor_list;
  gpu_monitor_list = new_node;
}


void
gpu_monitors_apply(gpu_monitor_apply_t *args_in, gpu_monitor_type_t type)
{
  gpu_monitor_node_t *node = gpu_monitor_list;

	while (node != NULL) {
    if (type == gpu_monitor_type_enter)
      node->enter_fn(node->component, args_in);
    else if (type == gpu_monitor_type_exit)
      node->exit_fn(node->component, args_in);

		node = node->next;
	}
}