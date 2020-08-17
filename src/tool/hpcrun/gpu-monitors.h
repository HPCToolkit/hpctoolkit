//
// Created by dejan on 7/15/20.
//

#ifndef HPCTOOLKIT_GPU_MONITORS_H
#define HPCTOOLKIT_GPU_MONITORS_H

#include "cct.h"

typedef void (*gpu_monitor_fn_t)(void* reg_info, void* args_in);

typedef enum {
	gpu_monitor_type_enter,
	gpu_monitor_type_exit
} gpu_monitor_type_t;


typedef struct gpu_monitors_apply_t {
	cct_node_t *cct_node;
  int (*gpu_sync_ptr)(void);
} gpu_monitors_apply_t;


typedef struct gpu_monitor_fn_entry_t {
	struct gpu_monitor_fn_entry_t* next;
	gpu_monitor_fn_t fn;
	void* reg_info;
} gpu_monitor_fn_entry_t;


extern void gpu_monitor_register(gpu_monitor_type_t type, gpu_monitor_fn_entry_t* entry);
extern void gpu_monitors_apply(void *args, gpu_monitor_type_t type);


#endif //HPCTOOLKIT_GPU_MONITORS_H
