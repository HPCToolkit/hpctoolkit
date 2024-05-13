// SPDX-FileCopyrightText: 2017-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _HPCTOOLKIT_DEVICE_FINALIZERS_H_
#define _HPCTOOLKIT_DEVICE_FINALIZERS_H_

typedef enum {
  device_finalizer_type_flush,
  device_finalizer_type_shutdown
} device_finalizer_type_t;


typedef void (*device_finalizer_fn_t)(void* args, int how);


typedef struct device_finalizer_fn_entry_t {
  struct device_finalizer_fn_entry_t* next;
  device_finalizer_fn_t fn;
  void* args;
} device_finalizer_fn_entry_t;


// note finalizers in the order they are registered
extern void device_finalizer_register(device_finalizer_type_t type, device_finalizer_fn_entry_t* entry);

// apply finalizers in the order they are registered
extern void device_finalizer_apply(device_finalizer_type_t type, int how);

#endif
