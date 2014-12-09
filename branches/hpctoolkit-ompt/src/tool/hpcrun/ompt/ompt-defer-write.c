/******************************************************************************
 * system includes
 *****************************************************************************/

#if 0
#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>
#endif

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>
#include <messages/messages.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/metrics.h>

#include "ompt-region.h"
#include "ompt-defer.h"
#include "ompt-defer-write.h"

#include <hpcrun/unresolved.h>
#include <hpcrun/write_data.h>

#include <ompt.h>

/******************************************************************************
 * entry variables and operations for delayed write *
 *****************************************************************************/

static struct entry_t *unresolved_list = NULL;

static spinlock_t unresolved_list_lock = SPINLOCK_UNLOCKED;

struct entry_t *
new_dw_entry()
{
  struct entry_t *entry = NULL;

  entry = (struct entry_t*)hpcrun_malloc(sizeof(struct entry_t));
  entry->prev = entry->next = NULL;
  entry->td = NULL;
  entry->flag = false;
  return entry;
}

void
insert_dw_entry(struct entry_t* entry)
{
  spinlock_lock(&unresolved_list_lock);
  if(!unresolved_list) {
    unresolved_list = entry;
    spinlock_unlock(&unresolved_list_lock);
    return;
  }
  entry->prev = NULL;
  entry->next = unresolved_list;
  unresolved_list->prev = entry;
  unresolved_list = entry;
  spinlock_unlock(&unresolved_list_lock);
}

void 
add_defer_td(thread_data_t *td)
{
  struct entry_t *entry = new_dw_entry();
  entry->td = td;
  insert_dw_entry(entry);
}

void
write_other_td()
{
  spinlock_lock(&unresolved_list_lock);
  struct entry_t *entry = unresolved_list;
  spinlock_unlock(&unresolved_list_lock);
  while(entry) {
    if(entry->flag) {
      entry = entry->next;
      continue;
    }
    entry->flag = true;
    thread_data_t *td = hpcrun_get_thread_data();
    cct2metrics_t* store_cct2metrics_map = td->core_profile_trace_data.cct2metrics_map;
    td->core_profile_trace_data.cct2metrics_map = entry->td->core_profile_trace_data.cct2metrics_map;
    if(entry->td->defer_flag) {
      TMSG(DEFER_CTXT, "write another td with id %d", entry->td->core_profile_trace_data.id);
      resolve_cntxt_fini(entry->td);
    }

    // write out a given td
    hpcrun_write_profile_data(&(entry->td->core_profile_trace_data));
    hpcrun_trace_close(&(entry->td->core_profile_trace_data));
    td->core_profile_trace_data.cct2metrics_map = store_cct2metrics_map;

    entry = entry->next;
  }
}
