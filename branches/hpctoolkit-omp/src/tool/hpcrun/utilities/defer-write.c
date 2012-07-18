/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <papi.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

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

#include <omp.h>
#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/metrics.h>
#include <utilities/defer-cntxt.h>
#include <utilities/defer-write.h>
#include <hpcrun/unresolved.h>
#include <hpcrun/write_data.h>

//#include "/home/xl10/support/gcc-4.6.2/libgomp/libgomp_g.h"
#include "hpcrun/libgomp/libgomp_g.h"

/******************************************************************************
 * entry variables and operations for delayed write *
 *****************************************************************************/

static struct entry_t *unresolved_list = NULL;
static struct entry_t *free_list = NULL;

static spinlock_t unresolved_list_lock = SPINLOCK_UNLOCKED;
static spinlock_t free_list_lock = SPINLOCK_UNLOCKED;

struct entry_t *
new_dw_entry()
{
  struct entry_t *entry = NULL;
  spinlock_lock(&free_list_lock);
  if(free_list) {
    entry = free_list;
    free_list = free_list->next;
    if(free_list) free_list->prev = NULL;
    entry->prev = entry->next = NULL;
    entry->td = NULL;
    entry->flag = false;
  }
  else {
    entry = (struct entry_t*)hpcrun_malloc(sizeof(struct entry_t));
    entry->prev = entry->next = NULL;
    entry->td = NULL;
    entry->flag = false;
  }
  spinlock_unlock(&free_list_lock);
  return entry;
}

thread_data_t *
get_reuse_td()
{
  spinlock_lock(&unresolved_list_lock);
  if(!unresolved_list) {
    spinlock_unlock(&unresolved_list_lock);
    return NULL;
  }
  struct entry_t *entry = unresolved_list;
  while(entry && entry->flag) entry=entry->next;
  if(!entry) {
    spinlock_unlock(&unresolved_list_lock);
    return NULL;
  }
  entry->flag = true;
  spinlock_unlock(&unresolved_list_lock);
  while(!entry->td->reuse) ;
  return entry->td;
}

void
delete_dw_entry(struct entry_t* entry)
{
#if 0
  // detach from the unresolved list
  spinlock_lock(&unresolved_list_lock);
  if(entry->prev) entry->prev->next = entry->next;
  if(entry->next) entry->next->prev = entry->prev;
  spinlock_unlock(&unresolved_list_lock);
  // insert to the head of the free list
  spinlock_lock(&free_list_lock);
  if(!free_list) {
    free_list = entry;
    spinlock_unlock(&free_list_lock);
    return;
  }
  entry->prev = NULL;
  entry->next = free_list;
  free_list->prev = entry;
  free_list = entry;
  spinlock_unlock(&free_list_lock);
#endif
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

struct entry_t*
fetch_dw_entry(struct entry_t **pointer)
{
  spinlock_lock(&unresolved_list_lock);
  if(!(*pointer)) {
    spinlock_unlock(&unresolved_list_lock);
    return NULL;
  }
  while((*pointer) && (*pointer)->flag) (*pointer) = (*pointer)->next;
  if((*pointer)) (*pointer)->flag = true;
  spinlock_unlock(&unresolved_list_lock);
  return (*pointer);
}

void
set_dw_pointer(struct entry_t **pointer)
{
  spinlock_lock(&unresolved_list_lock);
  *pointer = unresolved_list;
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
    if(entry->td->defer_flag)
      resolve_other_cntxt(entry->td);

    // write out a given td
    cct2metrics_t* store_cct2metrics_map = td->cct2metrics_map;
    td->cct2metrics_map = entry->td->cct2metrics_map;
    hpcrun_write_other_profile_data(entry->td->epoch, entry->td);
    trace_other_close((void *)entry->td);
    td->cct2metrics_map = store_cct2metrics_map;
 
    entry = entry->next;
  }
}
