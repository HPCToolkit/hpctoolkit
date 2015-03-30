//******************************************************************************
// local includes  
//******************************************************************************

#include "cct_backtrace_finalize.h"



//******************************************************************************
// global variables 
//******************************************************************************

static cct_backtrace_finalize_entry_t *finalizers;

#if 0
static cct_backtrace_null_handler_fn null_handler;
#endif

static cct_cursor_finalize_fn cursor_finalize;



//******************************************************************************
// interface operations
//******************************************************************************

void
cct_backtrace_finalize_register(
  cct_backtrace_finalize_entry_t *e
)
{
  // enqueue finalizer on list
  e->next = finalizers;
  finalizers = e;
}


void
cct_cursor_finalize_register(
  cct_cursor_finalize_fn fn
)
{
  cursor_finalize = fn;
}


void
cct_backtrace_finalize(
  backtrace_info_t *bt, 
  int isSync
)
{
 cct_backtrace_finalize_entry_t *e = finalizers;
  while (e) {
    e->fn(bt, isSync);
    e = e->next;
  }
}


#if 0
void
cct_backtrace_null_handler_register(
  cct_backtrace_null_handler_fn fn
)
{
  null_handler = fn;
}


cct_node_t *
cct_backtrace_null_handler(
  cct_node_t *n,
  void **trace_pc
)
{
  if (null_handler) return null_handler(n, trace_pc);
  else return n;
}
#endif


cct_node_t *
cct_cursor_finalize(
  cct_bundle_t *cct,
  backtrace_info_t *bt,
  cct_node_t *cursor
)
{
  if (cursor_finalize) return cursor_finalize(cct, bt, cursor);
  else return cursor;
}

