// SPDX-FileCopyrightText: 2017-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "device-finalizers.h"

static device_finalizer_fn_entry_t *kinds[2] = {0, 0};


void
device_finalizer_register(device_finalizer_type_t type, device_finalizer_fn_entry_t *entry)
{
   device_finalizer_fn_entry_t** device_fn = &kinds[type];

   // append finalizer at end of list.
   while (*device_fn != 0) {
     device_fn = &((*device_fn)->next);
   }
   *device_fn = entry;
}


void
device_finalizer_apply(device_finalizer_type_t type, int how)
{
   device_finalizer_fn_entry_t* fn = kinds[type];
   while (fn != 0) {
     fn->fn(fn->args, how);
     fn = fn->next;
   }
}
