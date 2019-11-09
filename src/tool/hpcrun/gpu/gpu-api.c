//
// Created by user on 19.8.2019..
//

#include "gpu-api.h"
#include <stdio.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <limits.h>    // PATH_MAX
// #include <pthread.h>
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir

#include <monitor.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <link.h>          // dl_iterate_phdr
#include <linux/limits.h>  // PATH_MAX
#include <string.h>        // strstr
#endif


#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/stdatomic.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/main.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/cct/cct.h>

void
gpu_correlation_callback
(
        uint64_t id,
        placeholder_t state,
        entry_data_t *entry_data,
	ip_normalized_t *kernel_ip
)
{
    hpcrun_metricVal_t zero_metric_incr = {.i = 0};
    int zero_metric_id = 0; // nothing to see here
    ucontext_t uc;
    getcontext(&uc);
    thread_data_t *td = hpcrun_get_thread_data();
    // NOTE(keren): hpcrun_safe_enter prevent self interruption
    hpcrun_safe_enter();

    cct_node_t *node =
            hpcrun_sample_callpath(&uc, zero_metric_id,
                                   zero_metric_incr, 0, 1, NULL).sample_node;

    hpcrun_safe_exit();

    // Compress callpath
    // Highest cupti node
    cct_addr_t *node_addr = hpcrun_cct_addr(node);
    static __thread uint16_t libhpcrun_id = 0;

    // The lowest module must be libhpcrun, which is not 0
    // Update libhpcrun_id only the first time
    if (libhpcrun_id == 0) {
        load_module_t* module = hpcrun_loadmap_findById(node_addr->ip_norm.lm_id);
        if (module != NULL && strstr(module->name, "libhpcrun") != NULL) {
            libhpcrun_id = node_addr->ip_norm.lm_id;
        }
    }

    // Skip libhpcrun
    while (libhpcrun_id != 0 && node_addr->ip_norm.lm_id == libhpcrun_id) {
        //hpcrun_cct_delete_self(node);
        node = hpcrun_cct_parent(node);
        node_addr = hpcrun_cct_addr(node);
    }

    // Skip libcupti and libcuda
    while (module_ignore_map_module_id_lookup(node_addr->ip_norm.lm_id)) {
        //hpcrun_cct_delete_self(node);
        node = hpcrun_cct_parent(node);
        node_addr = hpcrun_cct_addr(node);
    }

    td->overhead++;
    hpcrun_safe_enter();

    // Insert the corresponding cuda state node
    cct_addr_t frm_api;
    memset(&frm_api, 0, sizeof(cct_addr_t));
    frm_api.ip_norm = state.pc_norm;
    cct_node_t* cct_api = hpcrun_cct_insert_addr(node, &frm_api);

    if (kernel_ip) {
      cct_addr_t func_frm;
      memset(&func_frm, 0, sizeof(cct_addr_t));
      func_frm.ip_norm = *kernel_ip;
      cct_func = hpcrun_cct_insert_addr(cct_api, &func_frm);
      hpcrun_cct_retain(cct_func);
    }

    hpcrun_safe_exit();
    td->overhead--;

    // Generate notification entry
    correlation_produce(id, cct_api, cct_func);

}

