#include "device-finalizers.h"

static device_finalizer_fn_entry_t *device_fns = 0;


void
device_finalizer_register(device_finalizer_fn_entry_t *entry)
{
   entry->next = device_fns;
   device_fns = entry;
}


void 
device_finalizer_apply()
{
   device_finalizer_fn_entry_t* fn = device_fns;
   while (fn != 0) {
     fn->fn(fn->args);
     fn = fn->next;
   }
}
