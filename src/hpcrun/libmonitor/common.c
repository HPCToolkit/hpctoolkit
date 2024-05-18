// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Internal shared functions.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

#include <dlfcn.h>
#include <link.h>
#include <stdbool.h>

struct callback_data {
    const char* symbol;
    bool skip;
    void *skip_until_base;
    void *result;
};
static int phdr_callback(struct dl_phdr_info *info, size_t sz, void *data_v) {
    struct callback_data *data = data_v;

    // Decide whether to skip this object as being before libmonitor
    if (data->skip) {
        MONITOR_DEBUG("not scanning object: %s\n", info->dlpi_name);
        if (data->skip_until_base == (void*)info->dlpi_addr) {
            data->skip = false;
        }
        return 0;
    }

    // Call dlopen to stabilize a handle for our use
    void *this = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD | RTLD_LOCAL);
    if (this == NULL) {
        MONITOR_DEBUG("dlopen failed on object: %s\n", info->dlpi_name);
        return 0;
    }

    // Poke it and see if we can find the symbol we want. Stop if we found it,
    // otherwise continue the scan.
    data->result = dlsym(this, data->symbol);
    dlclose(this);
    return data->result == NULL ? 0 : 1;
}

void *monitor_dlsym(const char *symbol) {
    const char *err_str;
    dlerror();
    void *result = dlsym(RTLD_NEXT, symbol);
    err_str = dlerror();
    if (err_str != NULL) {
        // Fallback: try to find the symbol by inspecting every object loaded
        // after us. This is similar to but not identical to RTLD_NEXT, since we
        // also inspect objects that were loaded with dlopen(RTLD_LOCAL).

        struct callback_data cb_data = {
            .symbol = symbol,
            .skip = true,
            .skip_until_base = NULL,
            .result = NULL,
        };

        // Identify ourselves, using our base address
        Dl_info dli;
        if (dladdr(&monitor_dlsym, &dli) == 0) {
            // This should never happen, but if it does error
            MONITOR_ERROR1("dladdr1 failed to find libmonitor\n");
        }
        cb_data.skip_until_base = dli.dli_fbase;

        // Use dl_iterate_phdr to scan through all the objects
        int found = dl_iterate_phdr(phdr_callback, &cb_data);
        if (found == 0 || cb_data.result == NULL) {
            // We didn't find the required symbol, this is a hard error.
            MONITOR_ERROR("dlsym(%s) failed: %s\n", symbol, err_str);
        }
        result = cb_data.result;
    }
    MONITOR_DEBUG("%s() = %p\n", symbol, result);
    return result;
}
