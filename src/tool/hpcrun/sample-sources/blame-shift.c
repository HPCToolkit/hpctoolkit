#include "blame-shift.h"

static bs_fn_entry_t *bs_fns = 0;

void
blame_shift_register(bs_fn_entry_t *entry)
{
   entry->next = bs_fns;
   bs_fns = entry;
}

void 
blame_shift_apply(int metric_id, cct_node_t *node, int metric_incr)
{
   bs_fn_entry_t *fn = bs_fns;
   while(fn != 0) {
	fn->fn(metric_id, node, metric_incr);
	fn = fn->next;
   }
}
