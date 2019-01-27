#ifndef __ompt_types_h__ 

#define __ompt_types_h__ 

#include "../../../lib/prof-lean/stdatomic.h"
#include "../cct/cct.h"


#define FOREACH_OMPT_PLACEHOLDER_FN(macro)  \
    macro (ompt_idle)                       \
    macro (ompt_overhead)                   \
    macro (ompt_barrier_wait)               \
    macro (ompt_task_wait)                  \
    macro (ompt_mutex_wait)


struct ompt_base_s;


struct ompt_queue_data_s;
struct ompt_notification_s;
struct ompt_threads_queue_s;
struct ompt_thread_region_freelist_s;

// FIXME:
// create a union
//union{
// _Atomic(ompt_base_t*)anext
// ompt_base_t* next
//};

typedef union ompt_next_u ompt_next_t;
union ompt_next_u{
  struct ompt_base_s *next;
  _Atomic (struct ompt_base_s*)anext;
};

typedef struct ompt_base_s{
   ompt_next_t next; // FIXME: this should be Atomic too
}ompt_base_t;


#define ompt_base_nil (ompt_base_t*)0
#define ompt_base_invalid (ompt_base_t*)~0
// ompt_wfq_s
typedef struct ompt_wfq_s{
  _Atomic(ompt_base_t*)head;
}ompt_wfq_t;

typedef struct ompt_region_data_s {
  // region freelist, must be at the begin because od up/downcasting
  // like inherited class in C++, inheretes from base_t
  ompt_next_t next;
  // region's wait free queue
  ompt_wfq_t queue;
  // region's freelist which belongs to thread
  ompt_wfq_t *thread_freelist;
  // region id
  uint64_t region_id;
  // call_path to the region
  cct_node_t *call_path;
  // depth of the region, starts from zero
  int depth;
} ompt_region_data_t;

typedef struct ompt_notification_s{
  // it can also cover freelist to, we do not need another next_freelist
  ompt_next_t next;
  ompt_region_data_t *region_data;
  // struct ompt_threads_queue_s *threads_queue;
  ompt_wfq_t *threads_queue;
  // pointer to the cct pseudo node of the region that should be resolve
  cct_node_t* unresolved_cct;
} ompt_notification_t;

// trl = Thread's Regions List
// el  = element
typedef struct ompt_trl_el_s {
  // inheret from base_t
  ompt_next_t next;
  // previous in double-linked list
  struct ompt_trl_el_s* prev;
  // stores region's information
  ompt_region_data_t* region_data;
} ompt_trl_el_t;

extern int ompt_eager_context;

// region stack element which points to the corresponding
// notification, and says if thread took sample and if the
// thread is the master in team
typedef struct region_stack_el_s {
  ompt_notification_t *notification;
  bool took_sample;
  bool team_master;
  ompt_frame_t *parent_frame;
} region_stack_el_t;


#endif

// FIXME vi3: ompt_data_t freelist manipulation
