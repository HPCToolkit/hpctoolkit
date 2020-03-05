#include "sample-filters.h"

static sf_fn_entry_t *sf_filters = 0;

void
sample_filters_register(sf_fn_entry_t *entry)
{
   entry->next = sf_filters;
   sf_filters = entry;
}


int 
sample_filters_apply()
{
   sf_fn_entry_t* entry = sf_filters;
   while(entry != 0) {
     if (entry->fn(entry->arg)) return 1;
     entry = entry->next;
   }
   return 0;
}
