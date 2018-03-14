#ifndef _HPCTOOLKIT_DEVICE_FINALIZERS_H_
#define _HPCTOOLKIT_DEVICE_FINALIZERS_H_

typedef enum {
  device_finalizer_type_flush,
  device_finalizer_type_shutdown
} device_finalizer_type_t;


typedef void (*device_finalizer_fn_t)(void* args);


typedef struct device_finalizer_fn_entry_t {
  struct device_finalizer_fn_entry_t* next;
  device_finalizer_fn_t fn;
  void* args;
} device_finalizer_fn_entry_t;


extern void device_finalizer_register(device_finalizer_type_t type, device_finalizer_fn_entry_t* entry);
extern void device_finalizer_apply();

#endif
