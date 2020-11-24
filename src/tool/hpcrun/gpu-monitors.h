//
// Created by dejan on 7/15/20.
//

#ifndef HPCTOOLKIT_GPU_MONITORS_H
#define HPCTOOLKIT_GPU_MONITORS_H

#include "cct.h"


typedef enum {
	gpu_monitor_type_enter,
	gpu_monitor_type_exit
} gpu_monitor_type_t;


typedef struct gpu_monitor_apply_t {
	cct_node_t *cct_node;
  const char *name;
} gpu_monitor_apply_t;


typedef void (*gpu_monitor_fn_t)(const void* component, gpu_monitor_apply_t* args_in);

typedef struct gpu_monitor_node_t {
	struct gpu_monitor_node_t * next;
	void *component;
	gpu_monitor_fn_t enter_fn;
  gpu_monitor_fn_t exit_fn;
} gpu_monitor_node_t;


extern void gpu_monitor_register(gpu_monitor_node_t node);
extern void gpu_monitors_apply(gpu_monitor_apply_t *args, gpu_monitor_type_t type);


#endif //HPCTOOLKIT_GPU_MONITORS_H
