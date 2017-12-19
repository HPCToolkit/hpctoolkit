#ifndef device_finalizers_h
#define device_finalizers_h

typedef void (*device_finalizer_fn_t)(void* args);

typedef struct device_finalizer_fn_entry_t {
  struct device_finalizer_fn_entry_t* next;
  device_finalizer_fn_t fn;
  void* args;
} device_finalizer_fn_entry_t;

extern void device_finalizer_register(device_finalizer_fn_entry_t* entry);
extern void device_finalizer_apply();

#endif
