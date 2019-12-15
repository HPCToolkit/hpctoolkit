#include "device-finalizers.h"

static device_finalizer_fn_entry_t *kinds[2] = {0, 0};


void
device_finalizer_register(device_finalizer_type_t type, device_finalizer_fn_entry_t *entry)
{
   device_finalizer_fn_entry_t* device_fn = kinds[type];
   entry->next = device_fn;
   kinds[type] = entry;
}


void 
device_finalizer_apply(device_finalizer_type_t type)
{
   device_finalizer_fn_entry_t* fn = kinds[type];
   while (fn != 0) {
     fn->fn(fn->args);
     fn = fn->next;
   }
}
