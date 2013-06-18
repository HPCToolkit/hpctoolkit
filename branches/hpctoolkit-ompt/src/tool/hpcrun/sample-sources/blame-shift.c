#include "blame-shift.h"
#include "thread_data.h"

//------------------------------------------------------------------------------
// global variables
//------------------------------------------------------------------------------

static bs_tfn_entry_t *bs_tfns = 0;

static bs_fn_entry_t *bs_fns[2] = {0, 0};

static int bs_type_registered[] = {0, 0};

static int blame_shift_targets_allowed = 0;


//------------------------------------------------------------------------------
// private operations 
//------------------------------------------------------------------------------

static uint64_t
blame_shift_target_compute()
{
  uint64_t target = 0;
  bs_tfn_entry_t *fn = bs_tfns;
  if (fn) {
    while (fn != 0) {
      target = fn->fn();
      if (target) break;
      fn = fn->next;
    }
    if (target) directed_blame_shift_start(target);
    else directed_blame_shift_end(target);
  }
  return target;
}


static void 
blame_shift_apply_one(int metric_id, cct_node_t *node, int metric_incr, bs_type_t blame_type)
{
   bs_fn_entry_t *fn = bs_fns[blame_type];
   while (fn != 0) {
	fn->fn(metric_id, node, metric_incr);
	fn = fn->next;
   }
}



//------------------------------------------------------------------------------
// interface operations 
//------------------------------------------------------------------------------

uint64_t
blame_shift_target_get()
{
  thread_data_t *td = hpcrun_get_thread_data();
  return td->blame_target;
}

void
blame_shift_register(bs_fn_entry_t *entry, bs_type_t blame_type)
{
   entry->next = bs_fns[blame_type];
   bs_fns[blame_type] = entry;
}

void
blame_shift_target_allow()
{
  blame_shift_targets_allowed = 1;
}

void
blame_shift_target_register(bs_tfn_entry_t *entry)
{
  if (blame_shift_targets_allowed) {
    entry->next = bs_tfns;
    bs_tfns = entry;
  }
}


void 
blame_shift_apply(int metric_id, cct_node_t *node, int metric_incr)
{
  if (blame_shift_target_compute()) {
    blame_shift_apply_one(metric_id, node, metric_incr, bs_type_directed);
  } else blame_shift_apply_one(metric_id, node, metric_incr, bs_type_undirected);
}


void 
blame_shift_heartbeat_register(bs_heartbeat_t bst)
{
   bs_type_registered[bst] = 1;
}


int 
blame_shift_heartbeat_available(bs_heartbeat_t bst)
{
   return bs_type_registered[bst];
}

