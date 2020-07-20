//
// Created by dejan on 7/15/20.
//

#include "gpu-monitors.h"

static gpu_monitor_fn_entry_t *kinds[2] = {0, 0};
static const char *gpu_name[] = {"unknown", "nvidia", "amd", "intel"};


void
gpu_monitor_register(gpu_monitor_type_t type, gpu_monitor_fn_entry_t *entry)
{
	gpu_monitor_fn_entry_t* device_fn = kinds[type];
	entry->next = device_fn;
	kinds[type] = entry;
}


void
gpu_monitors_apply(void *args_in, gpu_monitor_type_t type)
{
	gpu_monitor_fn_entry_t* fn = kinds[type];
	while (fn != 0) {
		fn->fn(fn->reg_info, args_in);
		fn = fn->next;
	}
}

char *
gpu_monitors_get_gpu_name(gpu_type_t t)
{
	return gpu_name[t];
}