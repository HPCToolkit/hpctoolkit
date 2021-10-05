//
// Created by dejan on 7/15/20.
//

#ifndef HPCTOOLKIT_GPU_MONITORS_H
#define HPCTOOLKIT_GPU_MONITORS_H

#include <cct.h>
#include <sample-sources/papi-c.h>



typedef enum {
	gpu_monitor_type_enter,
	gpu_monitor_type_exit
} gpu_monitor_type_t;


typedef void (*gpu_monitor_enter_fn_t)(papi_component_info_t *ci, cct_node_t **cct_nodes, uint32_t num_unfinished_kernels);
typedef void (*gpu_monitor_exit_fn_t)(papi_component_info_t *ci);


typedef struct gpu_monitor_node_t {
	struct gpu_monitor_node_t * next;
  papi_component_info_t *ci;
  gpu_monitor_enter_fn_t enter_fn;
  gpu_monitor_exit_fn_t exit_fn;
} gpu_monitor_node_t;


extern void gpu_monitor_register(gpu_monitor_node_t node);
extern void gpu_monitors_apply(cct_node_t **cct_nodes, uint32_t num_unfinished_kernels, gpu_monitor_type_t type);


#endif //HPCTOOLKIT_GPU_MONITORS_H
