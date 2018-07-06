/*
 * include/41/ompt.h.var
 */

#ifndef __OMPT__
#define __OMPT__

/*****************************************************************************
 * system include files
 *****************************************************************************/

#include <stdint.h>
#include <lib/prof-lean/spinlock.h>




/*****************************************************************************
 * iteration macros
 *****************************************************************************/

#define FOREACH_OMPT_INQUIRY_FN(macro)  \
    macro (ompt_enumerate_states)       \
    macro (ompt_enumerate_mutex_impls)  \
                                        \
    macro (ompt_set_callback)           \
    macro (ompt_get_callback)           \
                                        \
    macro (ompt_get_state)              \
                                        \
    macro (ompt_get_parallel_info)      \
    macro (ompt_get_task_info)          \
    macro (ompt_get_thread_data)        \
    macro (ompt_get_unique_id)          \
                                        \
    macro(ompt_get_num_places)          \
    macro(ompt_get_place_proc_ids)      \
    macro(ompt_get_place_num)           \
    macro(ompt_get_partition_place_nums)\
    macro(ompt_get_proc_id)


#define FOREACH_OMPT_PLACEHOLDER_FN(macro)  \
    macro (ompt_idle)                       \
    macro (ompt_overhead)                   \
    macro (ompt_barrier_wait)               \
    macro (ompt_task_wait)                  \
    macro (ompt_mutex_wait)



#define FOREACH_OMP_STATE(macro)                                                                \
                                                                                                \
    /* first available state */                                                                 \
    macro (omp_state_undefined, 0x102)      /* undefined thread state */                        \
                                                                                                \
    /* work states (0..15) */                                                                   \
    macro (omp_state_work_serial, 0x000)    /* working outside parallel */                      \
    macro (omp_state_work_parallel, 0x001)  /* working within parallel */                       \
    macro (omp_state_work_reduction, 0x002) /* performing a reduction */                        \
                                                                                                \
    /* barrier wait states (16..31) */                                                          \
    macro (omp_state_wait_barrier, 0x010)   /* waiting at a barrier */                          \
    macro (omp_state_wait_barrier_implicit_parallel, 0x011)                                     \
                                            /* implicit barrier at the end of parallel region */\
    macro (omp_state_wait_barrier_implicit_workshare, 0x012)                                    \
                                            /* implicit barrier at the end of worksharing */    \
    macro (omp_state_wait_barrier_implicit, 0x013)  /* implicit barrier */                      \
    macro (omp_state_wait_barrier_explicit, 0x014)  /* explicit barrier */                      \
                                                                                                \
    /* task wait states (32..63) */                                                             \
    macro (omp_state_wait_taskwait, 0x020)  /* waiting at a taskwait */                         \
    macro (omp_state_wait_taskgroup, 0x021) /* waiting at a taskgroup */                        \
                                                                                                \
    /* mutex wait states (64..127) */                                                           \
    macro (omp_state_wait_mutex, 0x040)                                                         \
    macro (omp_state_wait_lock, 0x041)      /* waiting for lock */                              \
    macro (omp_state_wait_critical, 0x042)  /* waiting for critical */                          \
    macro (omp_state_wait_atomic, 0x043)    /* waiting for atomic */                            \
    macro (omp_state_wait_ordered, 0x044)   /* waiting for ordered */                           \
                                                                                                \
    /* target wait states (128..255) */                                                         \
    macro (omp_state_wait_target, 0x080)        /* waiting for critical */                      \
    macro (omp_state_wait_target_map, 0x081)    /* waiting for atomic */                        \
    macro (omp_state_wait_target_update, 0x082) /* waiting for ordered */                       \
                                                                                                \
    /* misc (256..511) */                                                                       \
    macro (omp_state_idle, 0x100)           /* waiting for work */                              \
    macro (omp_state_overhead, 0x101)       /* overhead excluding wait states */


#define FOREACH_OMPT_EVENT(macro)                                                                               \
                                                                                                                \
    /*--- Mandatory Events ---*/                                                                                \
    macro (ompt_callback_thread_begin,          ompt_callback_thread_begin_t,   1) /* thread begin */           \
    macro (ompt_callback_thread_end,            ompt_callback_thread_end_t,     2) /* thread end */             \
                                                                                                                \
    macro (ompt_callback_parallel_begin,        ompt_callback_parallel_begin_t, 3) /* parallel begin */         \
    macro (ompt_callback_parallel_end,          ompt_callback_parallel_end_t,   4) /* parallel end */           \
                                                                                                                \
    macro (ompt_callback_task_create,           ompt_callback_task_create_t,       5) /* task begin */             \
    macro (ompt_callback_implicit_task,         ompt_callback_implicit_task_t,  7) /* implicit task   */        \
                                                                                                                \
    macro (ompt_callback_mutex_released,        ompt_callback_mutex_t,         15) /* lock release */           \
    macro (ompt_callback_idle,                  ompt_callback_idle_t,          13) /* begin or end idle state */\
    macro (ompt_callback_sync_region_wait,      ompt_callback_sync_region_t,   14) /* sync region wait begin or end*/  \
    macro (ompt_callback_sync_region,           ompt_callback_sync_region_t,   21) /* sync region begin or end */



/*****************************************************************************
 * data types
 *****************************************************************************/

/*---------------------
 * identifiers
 *---------------------*/


// ----
typedef uint64_t ompt_id_t;
#define ompt_id_none 0

typedef union ompt_data_u {
    uint64_t value;
    void *ptr;
} ompt_data_t;

static const ompt_data_t ompt_data_none = {0};
// -----

typedef uint64_t ompt_thread_id_t;
#define ompt_thread_id_none ((ompt_thread_id_t) 0)     /* non-standard */

typedef uint64_t ompt_task_id_t;
#define ompt_task_id_none ((ompt_task_id_t) 0)         /* non-standard */

typedef uint64_t ompt_parallel_id_t;
#define ompt_parallel_id_none ((ompt_parallel_id_t) 0) /* non-standard */

typedef uint64_t ompt_wait_id_t;
#define ompt_wait_id_none ((ompt_wait_id_t) 0)         /* non-standard */



// -----
typedef void ompt_device_t;
// -----


/*---------------------
 * ompt_frame_t
 *---------------------*/

typedef struct ompt_frame_s {
    void *exit_runtime_frame;    /* next frame is user code     */
    void *reenter_runtime_frame; /* previous frame is user code */
} ompt_frame_t;


/*****************************************************************************
 * enumerations for thread states and runtime events
 *****************************************************************************/

/*---------------------
 * runtime states
 *---------------------*/

typedef enum {
#define omp_state_macro(state, code) state = code,
    FOREACH_OMP_STATE(omp_state_macro)
#undef omp_state_macro
} omp_state_t;


/*---------------------
 * runtime events
 *---------------------*/

typedef enum ompt_callbacks_e{
#define ompt_event_macro(event, callback, eventid) event = eventid,
    FOREACH_OMPT_EVENT(ompt_event_macro)
#undef ompt_event_macro
} ompt_callbacks_t;


/*---------------------
 * set callback results
 *---------------------*/
typedef enum {
    ompt_set_result_registration_error              = 0,
    ompt_set_result_event_may_occur_no_callback     = 1,
    ompt_set_result_event_never_occurs              = 2,
    ompt_set_result_event_may_occur_callback_some   = 3,
    ompt_set_result_event_may_occur_callback_always = 4,
} ompt_set_result_t;



/*****************************************************************************
 * callback signatures
 *****************************************************************************/

/* initialization */
typedef void (*ompt_interface_fn_t)(void);

typedef ompt_interface_fn_t (*ompt_function_lookup_t)(
    const char * interface_function_name                     /* entry point to look up       */
);

/* threads */

typedef enum {
    ompt_thread_initial = 1, // start the enumeration at 1
    ompt_thread_worker  = 2,
    ompt_thread_other   = 3,
    ompt_thread_unknown = 4
} ompt_thread_type_t;


typedef enum {
    ompt_invoker_program = 1,         /* program invokes master task  */
    ompt_invoker_runtime = 2          /* runtime invokes master task  */
} ompt_invoker_t;


typedef void (*ompt_callback_thread_begin_t)(
    ompt_thread_type_t thread_type,      /*type of thread */
    ompt_data_t *thread_data             /* data of thread */
);


typedef void (*ompt_callback_thread_end_t) (
    ompt_data_t *thread_data
);


typedef void (*ompt_wait_callback_t) (
    ompt_wait_id_t wait_id            /* wait id                      */
);

/* parallel and workshares */
typedef void (*ompt_parallel_callback_t) (
    ompt_parallel_id_t parallel_id,    /* id of parallel region       */
    ompt_task_id_t task_id             /* id of task                  */
);

typedef enum ompt_scope_endpoint_e {
    ompt_scope_begin                    = 1,
    ompt_scope_end                      = 2
} ompt_scope_endpoint_t;

typedef void (*ompt_callback_implicit_task_t) (
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  unsigned int team_size,
  unsigned int thread_num
);


typedef void (*ompt_new_workshare_callback_t) (
    ompt_parallel_id_t parallel_id,   /* id of parallel region        */
    ompt_task_id_t parent_task_id,    /* id of parent task            */
    void *workshare_function          /* pointer to outlined function */
);


/* implicit tasks */

typedef void (*ompt_callback_parallel_begin_t) (
    ompt_data_t *parent_task_data,
    const ompt_frame_t *parent_frame,
    ompt_data_t *parallel_data,
    unsigned int requested_team_size,
    ompt_invoker_t invoker,
    const void *codeptr_ra
);

typedef void (*ompt_callback_parallel_end_t) (
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    ompt_invoker_t invoker,
    const void *codeptr_ra
);


/* tasks */
typedef enum ompt_task_type_e {
    ompt_task_initial = 0x1,
    ompt_task_implicit = 0x2,
    ompt_task_explicit = 0x4,
    ompt_task_target = 0x8,
    ompt_task_undeferred = 0x8000000,
    ompt_task_untied = 0x10000000,
    ompt_task_final = 0x20000000,
    ompt_task_mergeable = 0x40000000,
    ompt_task_merged = 0x80000000
} ompt_task_type_t;


typedef void (*ompt_task_callback_t) (
    ompt_task_id_t task_id            /* id of task                   */
);

typedef void (*ompt_task_switch_callback_t) (
    ompt_task_id_t suspended_task_id, /* tool data for suspended task */
    ompt_task_id_t resumed_task_id    /* tool data for resumed task   */
);

//typedef void (*ompt_new_task_callback_t) (
//    ompt_task_id_t parent_task_id,    /* id of parent task            */
//    ompt_frame_t *parent_task_frame,  /* frame data for parent task   */
//    ompt_task_id_t  new_task_id,      /* id of created task           */
//    void *task_function               /* pointer to outlined function */
//);


typedef void (*ompt_callback_task_create_t) (
    ompt_data_t *parent_task_data,
    const ompt_frame_t *parent_frame,
    ompt_data_t *new_task_data,
    ompt_task_type_t type,
    int has_dependences,
    const void *codeptr_ra
);


/* program */
typedef void (*ompt_control_callback_t) (
    uint64_t command,                 /* command of control call      */
    uint64_t modifier                 /* modifier of control call     */
);

typedef void (*ompt_callback_t)(void);





/* control_tool */


typedef enum ompt_mutex_kind_e {
    ompt_mutex              = 0x10,
    ompt_mutex_lock         = 0x11,
    ompt_mutex_nest_lock    = 0x12,
    ompt_mutex_critical     = 0x13,
    ompt_mutex_atomic       = 0x14,
    ompt_mutex_ordered      = 0x20
} ompt_mutex_kind_t;


typedef void (*ompt_callback_mutex_t) (
  ompt_mutex_kind_t kind,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra
);

typedef void (*ompt_callback_idle_t) (
  ompt_scope_endpoint_t endpoint
);

typedef enum ompt_sync_region_kind_e {
    ompt_sync_region_barrier = 1,
    ompt_sync_region_taskwait = 2,
    ompt_sync_region_taskgroup = 3
} ompt_sync_region_kind_t;

typedef void (*ompt_callback_sync_region_t) (
  ompt_sync_region_kind_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra
);




/****************************************************************************
 * ompt API
 ***************************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif

#define OMPT_API_FNTYPE(fn) fn##_t

#define OMPT_API_FUNCTION(return_type, fn, args)  \
    typedef return_type (*OMPT_API_FNTYPE(fn)) args



/****************************************************************************
 * INQUIRY FUNCTIONS
 ***************************************************************************/

/* state */
OMPT_API_FUNCTION(omp_state_t, ompt_get_state, (
    ompt_wait_id_t *ompt_wait_id
));


/* thread */
OMPT_API_FUNCTION(ompt_thread_id_t, ompt_get_thread_id, (void));

OMPT_API_FUNCTION(void *, ompt_get_idle_frame, (void));

OMPT_API_FUNCTION(ompt_data_t*, ompt_get_thread_data, (void));

/* parallel region */
OMPT_API_FUNCTION(ompt_parallel_id_t, ompt_get_parallel_id, (
    int ancestor_level
));

OMPT_API_FUNCTION(int, ompt_get_parallel_team_size, (
    int ancestor_level
));

OMPT_API_FUNCTION(int, ompt_get_parallel_info, (
    int ancestor_level,
    ompt_data_t **parallel_data,
    int *team_size
));

/* task */
OMPT_API_FUNCTION(ompt_task_id_t, ompt_get_task_id, (
    int depth
));

OMPT_API_FUNCTION(ompt_frame_t *, ompt_get_task_frame, (
    int depth
));

OMPT_API_FUNCTION(int, ompt_get_task_info, (
    int ancestor_level,
    ompt_task_type_t *type,
    ompt_data_t **task_data,
    ompt_frame_t **task_frame,
    ompt_data_t **parallel_data,
    int *thread_num
));

/* places */
OMPT_API_FUNCTION(int, ompt_get_num_places, (void));

OMPT_API_FUNCTION(int, ompt_get_place_proc_ids, (
  int place_num,
  int ids_size,
  int *ids
));

OMPT_API_FUNCTION(int, ompt_get_place_num, (void));

OMPT_API_FUNCTION(int, ompt_get_partition_place_nums, (
  int place_nums_size,
  int *place_nums
));

/* proc_id */
OMPT_API_FUNCTION(int, ompt_get_proc_id, (void));




/****************************************************************************
 * PLACEHOLDERS FOR PERFORMANCE REPORTING
 ***************************************************************************/

/* idle */
OMPT_API_FUNCTION(void, ompt_idle, (
    void
));

/* overhead */
OMPT_API_FUNCTION(void, ompt_overhead, (
    void
));

/* barrier wait */
OMPT_API_FUNCTION(void, ompt_barrier_wait, (
    void
));

/* task wait */
OMPT_API_FUNCTION(void, ompt_task_wait, (
    void
));

/* mutex wait */
OMPT_API_FUNCTION(void, ompt_mutex_wait, (
    void
));



/****************************************************************************
 * INITIALIZATION FUNCTIONS
 ***************************************************************************/

typedef struct ompt_fns_t ompt_fns_t;

// replace ompt_initilizlieze with ompt_initialize_t
OMPT_API_FUNCTION(int, ompt_initialize, (
    ompt_function_lookup_t lookup,
    ompt_fns_t *fns)
);

typedef void (*ompt_finalize_t)(
    ompt_fns_t *fns
);


typedef struct ompt_fns_t {
    ompt_initialize_t initialize;
    ompt_finalize_t finalize;
} ompt_fns_t;


/* initialization interface to be defined by tool */
ompt_fns_t *ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version
);


typedef enum opt_init_mode_e {
    ompt_init_mode_never  = 0,
    ompt_init_mode_false  = 1,
    ompt_init_mode_true   = 2,
    ompt_init_mode_always = 3
} ompt_init_mode_t;



OMPT_API_FUNCTION(int, ompt_set_callback, (
    ompt_callbacks_t which,
    ompt_callback_t  callback
));


typedef enum ompt_set_callback_rc_e {  /* non-standard */
    ompt_set_callback_error      = 0,
    ompt_has_event_no_callback   = 1,
    ompt_no_event_no_callback    = 2,
    ompt_has_event_may_callback  = 3,
    ompt_has_event_must_callback = 4,
} ompt_set_callback_rc_t;


OMPT_API_FUNCTION(int, ompt_get_callback, (
    ompt_callbacks_t which,
    ompt_callback_t *callback
));


/****************************************************************************
 * MISCELLANEOUS FUNCTIONS
 ***************************************************************************/

/* control */
#if defined(_OPENMP) && (_OPENMP >= 201307)
#pragma omp declare target
#endif
void ompt_control(
    uint64_t command,
    uint64_t modifier
);
#if defined(_OPENMP) && (_OPENMP >= 201307)
#pragma omp end declare target
#endif

/* state enumeration */

/* state enumeration */
OMPT_API_FUNCTION(int, ompt_enumerate_states, (
  int current_state,
  int *next_state,
  const char **next_state_name
));


/* mutex implementation enumeration */
OMPT_API_FUNCTION(int, ompt_enumerate_mutex_impls, (
  int current_impl,
  int *next_impl,
  const char **next_impl_name
));




OMPT_API_FUNCTION(uint64_t, ompt_get_unique_id, (void));



#ifdef  __cplusplus
};
#endif

#include "../../../lib/prof-lean/stdatomic.h"
#include "../cct/cct.h"

//typedef struct cct_node_t cct_node_t;

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
} ompt_region_data_t;

typedef struct ompt_notification_s{
  // it can also cover freelist to, we do not need another next_freelist
  ompt_next_t next;
  ompt_region_data_t *region_data;
  // struct ompt_threads_queue_s *threads_queue;
  ompt_wfq_t *threads_queue;
} ompt_notification_t;

// trl = Thread's Regions List
// el  = element
typedef struct ompt_trl_el_s{
  // inheret from base_t
  ompt_next_t next;
  // previous in double-linked list
  struct ompt_trl_el_s* prev;
  // stores region's information
  ompt_region_data_t* region_data;
} ompt_trl_el_t;

extern int ompt_eager_context;

#endif

// FIXME vi3: ompt_data_t freelist manipulation


// ./autogen and add file in Makefile.am
// https://github.com/HPCToolkit/hpctoolkit-devtools/tree/master/autotools
// hpctoolkit-devtools at github where autogen and other are implemented
