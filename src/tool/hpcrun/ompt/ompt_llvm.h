/*
 * include/50/ompt.h.var
 */

#ifndef __OMPT__
#define __OMPT__

/*****************************************************************************
 * system include files
 *****************************************************************************/

#include <stdint.h>
#include <stddef.h>



/*****************************************************************************
 * iteration macros
 *****************************************************************************/

#define FOREACH_OMPT_INQUIRY_FN(macro)      \
    macro (ompt_enumerate_states)           \
    macro (ompt_enumerate_mutex_impls)      \
                                            \
    macro (ompt_set_callback)               \
    macro (ompt_get_callback)               \
                                            \
    macro (ompt_get_state)                  \
                                            \
    macro (ompt_get_parallel_info)          \
    macro (ompt_get_task_info)              \
    macro (ompt_get_thread_data)            \
    macro (ompt_get_unique_id)              \
                                            \
    macro(ompt_get_num_places)              \
    macro(ompt_get_place_proc_ids)          \
    macro(ompt_get_place_num)               \
    macro(ompt_get_partition_place_nums)    \
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
    macro (omp_state_overhead, 0x101)       /* overhead excluding wait states */                \
                                                                                                \
    /* implementation-specific states (512..) */


#define FOREACH_OMPT_MUTEX_IMPL(macro)                                                \
    macro (ompt_mutex_impl_unknown, 0)      /* unknown implementatin */               \
    macro (ompt_mutex_impl_spin, 1)         /* based on spin */                       \
    macro (ompt_mutex_impl_queuing, 2)      /* based on some fair policy */           \
    macro (ompt_mutex_impl_speculative, 3)  /* based on HW-supported speculation */

#define FOREACH_OMPT_EVENT(macro)                                                                               \
                                                                                                                \
    /*--- Mandatory Events ---*/                                                                                \
    macro (ompt_callback_thread_begin,          ompt_callback_thread_begin_t,   1) /* thread begin */           \
    macro (ompt_callback_thread_end,            ompt_callback_thread_end_t,     2) /* thread end */             \
                                                                                                                \
    macro (ompt_callback_parallel_begin,        ompt_callback_parallel_begin_t, 3) /* parallel begin */         \
    macro (ompt_callback_parallel_end,          ompt_callback_parallel_end_t,   4) /* parallel end */           \
                                                                                                                \
    macro (ompt_callback_task_create,           ompt_callback_task_create_t,    5) /* task begin */             \
    macro (ompt_callback_task_schedule,         ompt_callback_task_schedule_t,  6) /* task schedule */          \
    macro (ompt_callback_implicit_task,         ompt_callback_implicit_task_t,  7) /* implicit task   */        \
                                                                                                                \
    /*macro (ompt_callback_target,                ompt_callback_target_t,         8)*/ /* target */                 \
    /*macro (ompt_callback_target_data_op         ompt_callback_target_data_op_t, 9)*/ /* target data op*/          \
    /*macro (ompt_callback_target_submit,         ompt_callback_target_submit_t, 10)*/ /* target  submit*/          \
                                                                                                                \
    macro (ompt_callback_control_tool,          ompt_callback_control_tool_t,  11) /* control tool */           \
                                                                                                                \
    /*macro (ompt_callback_device_initialize,     ompt_callback_device_initialize_t, 12)*/ /* device initialize */  \
                                                                                                                \
    /*--- Optional Events (blame shifting, ompt_event_unimplemented) ---*/                                      \
    macro (ompt_callback_idle,                  ompt_callback_idle_t,          13) /* begin or end idle state */\
                                                                                                                \
    macro (ompt_callback_sync_region_wait,      ompt_callback_sync_region_t,   14) /* sync region wait begin or end*/  \
                                                                                                                \
    macro (ompt_callback_mutex_released,        ompt_callback_mutex_t,         15) /* mutex released */         \
                                                                                                                \
    /*--- Optional Events (synchronous events, ompt_event_unimplemented) --- */                                 \
                                                                                                                \
    macro (ompt_callback_task_dependences,      ompt_callback_task_dependences_t, 16) /* report task dependences  */\
    macro (ompt_callback_task_dependence,       ompt_callback_task_dependence_t, 17) /* report task dependence  */\
                                                                                                                \
    macro (ompt_callback_work,                  ompt_callback_work_t,          18) /* task at work begin or end*/\
                                                                                                                \
    macro (ompt_callback_master,                ompt_callback_master_t,        19) /* task at master begin or end */\
                                                                                                                \
    /*macro (ompt_callback_target_map,            ompt_callback_target_map_t,    20)*/ /* target map */             \
                                                                                                                \
    macro (ompt_callback_sync_region,           ompt_callback_sync_region_t,   21) /* sync region begin or end */ \
                                                                                                                \
    macro (ompt_callback_lock_init,             ompt_callback_mutex_acquire_t, 22) /* lock init */              \
    macro (ompt_callback_lock_destroy,          ompt_callback_mutex_t,         23) /* lock destroy */           \
                                                                                                                \
    macro (ompt_callback_mutex_acquire,         ompt_callback_mutex_acquire_t, 24) /* mutex acquire */          \
    macro (ompt_callback_mutex_acquired,        ompt_callback_mutex_t,         25) /* mutex acquired */         \
                                                                                                                \
    macro (ompt_callback_nest_lock,             ompt_callback_nest_lock_t,     26) /* nest lock */              \
                                                                                                                \
    macro (ompt_callback_flush,                 ompt_callback_flush_t,         27) /* after executing flush */  \
                                                                                                                \
    macro (ompt_callback_cancel,                ompt_callback_cancel_t,        28) /*cancel innermost binding region*/\



/*****************************************************************************
 * data types
 *****************************************************************************/

/*---------------------
 * identifiers
 *---------------------*/

typedef uint64_t ompt_id_t;
#define ompt_id_none 0

typedef union ompt_data_u {
  uint64_t value; /* data initialized by runtime to unique id */
  void *ptr;      /* pointer under tool control */
} ompt_data_t;

static const ompt_data_t ompt_data_none = {0};

typedef uint64_t ompt_wait_id_t;
static const ompt_wait_id_t ompt_wait_id_none = 0;


/*---------------------
 * ompt_frame_t
 *---------------------*/

typedef struct ompt_frame_s {
    void *exit_runtime_frame;    /* next frame is user code     */
    void *reenter_runtime_frame; /* previous frame is user code */
} ompt_frame_t;


/*---------------------
 * dependences types
 *---------------------*/

typedef enum ompt_task_dependence_flag_e {
    // a two bit field for the dependence type
    ompt_task_dependence_type_out   = 1,
    ompt_task_dependence_type_in    = 2,
    ompt_task_dependence_type_inout = 3,
} ompt_task_dependence_flag_t;

typedef struct ompt_task_dependence_s {
    void *variable_addr;
    uint32_t  dependence_flags;
} ompt_task_dependence_t;


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
typedef enum ompt_set_result_e {
    ompt_set_error = 0,
    ompt_set_never = 1,
    ompt_set_sometimes = 2,
    ompt_set_sometimes_paired = 3,
    ompt_set_always = 4
} ompt_set_result_t;


/*----------------------
 * mutex implementations
 *----------------------*/
typedef enum ompt_mutex_impl_e {
#define ompt_mutex_impl_macro(impl, code) impl = code,
    FOREACH_OMPT_MUTEX_IMPL(ompt_mutex_impl_macro)
#undef ompt_mutex_impl_macro
} ompt_mutex_impl_t;


/*****************************************************************************
 * callback signatures
 *****************************************************************************/

/* initialization */
typedef void (*ompt_interface_fn_t)(void);

typedef ompt_interface_fn_t (*ompt_function_lookup_t)(
    const char *                      /* entry point to look up       */
);

/* threads */
typedef enum {
    ompt_thread_initial = 1, // start the enumeration at 1
    ompt_thread_worker  = 2,
    ompt_thread_other   = 3
} ompt_thread_type_t;

typedef enum {
    ompt_invoker_program = 1,         /* program invokes master task  */
    ompt_invoker_runtime = 2          /* runtime invokes master task  */
} ompt_invoker_t;

typedef void (*ompt_callback_thread_begin_t) (
    ompt_thread_type_t thread_type,   /* type of thread               */
    ompt_data_t *thread_data          /* data of thread               */
);

typedef void (*ompt_callback_thread_end_t) (
    ompt_data_t *thread_data          /* data of thread               */
);

typedef void (*ompt_wait_callback_t) (
    ompt_wait_id_t wait_id        /* wait data                    */
);

/* parallel and workshares */
typedef enum ompt_scope_endpoint_e {
    ompt_scope_begin = 1,
    ompt_scope_end = 2
} ompt_scope_endpoint_t;


/* implicit task */
typedef void (*ompt_callback_implicit_task_t) (
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    unsigned int team_size,
    unsigned int thread_num
);

typedef void (*ompt_callback_parallel_begin_t) (
    ompt_data_t *parent_task_data,    /* data of parent task         */
    const ompt_frame_t *parent_frame, /* frame data of parent task    */
    ompt_data_t *parallel_data,       /* data of parallel region   */
    unsigned int requested_team_size, /* requested number of threads in team    */
    ompt_invoker_t invoker,           /* who invokes master task?     */
    const void *codeptr_ra            
);

typedef void (*ompt_callback_parallel_end_t) (
    ompt_data_t *parallel_data,        /* data of parallel region    */
    ompt_data_t *task_data,            /* data of task                 */
    ompt_invoker_t invoker,            /* who invokes master task?     */ 
    const void *codeptr_ra             
);

/* tasks */
typedef enum ompt_task_type_e {
    ompt_task_initial    = 0x1,
    ompt_task_implicit   = 0x2,
    ompt_task_explicit   = 0x4,
    ompt_task_target     = 0x8,
    ompt_task_undeferred = 0x8000000,
    ompt_task_untied     = 0x10000000,
    ompt_task_final      = 0x20000000,
    ompt_task_mergeable  = 0x40000000,
    ompt_task_merged     = 0x80000000
} ompt_task_type_t;

typedef enum ompt_task_status_e {
    ompt_task_complete = 1,
    ompt_task_yield    = 2,
    ompt_task_cancel   = 3,
    ompt_task_others   = 4
} ompt_task_status_t;

typedef void (*ompt_callback_task_schedule_t) (
    ompt_data_t *first_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t *second_task_data
);

typedef void (*ompt_callback_task_create_t) (
    ompt_data_t *parent_task_data,    /* data of parent task          */
    const ompt_frame_t *parent_frame, /* frame data for parent task   */
    ompt_data_t *new_task_data,       /* data of created task         */
    int type,
    int has_dependences,
    const void *codeptr_ra
);

/* task dependences */
typedef void (*ompt_callback_task_dependences_t) (
    ompt_data_t *task_data,
    const ompt_task_dependence_t *deps,
    int ndeps
);

typedef void (*ompt_callback_task_dependence_t) (
    ompt_data_t *src_task_data,
    ompt_data_t *sink_task_data
);

/* control_tool */
typedef int (*ompt_callback_control_tool_t) (
    uint64_t command,                 /* command of control call      */
    uint64_t modifier,                /* modifier of control call     */
    void *arg,
    const void *codeptr_ra
);

typedef void (*ompt_callback_t)(void);

typedef enum ompt_mutex_kind_e {
    ompt_mutex = 0x10,
    ompt_mutex_lock = 0x11,
    ompt_mutex_nest_lock = 0x12,
    ompt_mutex_critical = 0x13,
    ompt_mutex_atomic = 0x14,
    ompt_mutex_ordered = 0x20
} ompt_mutex_kind_t;

typedef void (*ompt_callback_mutex_acquire_t) (
    ompt_mutex_kind_t kind,
    unsigned int hint,
    unsigned int impl,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra
);

typedef void (*ompt_callback_mutex_t) (
    ompt_mutex_kind_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra
);

typedef void (*ompt_callback_nest_lock_t) (
    ompt_scope_endpoint_t endpoint,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra
);

typedef void (*ompt_callback_master_t) (
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    const void *codeptr_ra
);

typedef void (*ompt_callback_idle_t) (
    ompt_scope_endpoint_t endpoint
);

typedef enum ompt_work_type_e {
    ompt_work_loop = 1,
    ompt_work_sections = 2,
    ompt_work_single_executor = 3,
    ompt_work_single_other = 4,
    ompt_work_workshare = 5,
    ompt_work_distribute = 6,
    ompt_work_taskloop = 7
} ompt_work_type_t;

typedef void (*ompt_callback_work_t) (
    ompt_work_type_t wstype,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    uint64_t count,
    const void *codeptr_ra
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

typedef enum ompt_cancel_flag_e {
    ompt_cancel_parallel       = 0x1,
    ompt_cancel_sections       = 0x2,
    ompt_cancel_do             = 0x4,
    ompt_cancel_taskgroup      = 0x8,
    ompt_cancel_activated      = 0x10,
    ompt_cancel_detected       = 0x20,
    ompt_cancel_discarded_task = 0x40
} ompt_cancel_flag_t;

typedef void (*ompt_callback_cancel_t) (
    ompt_data_t *task_data,
    int flags,
    const void *codeptr_ra
);

typedef void (*ompt_callback_flush_t) (
    ompt_data_t *thread_data,
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
    ompt_wait_id_t *wait_id
));

/* thread */
OMPT_API_FUNCTION(ompt_data_t*, ompt_get_thread_data, (void));

/* parallel region */
OMPT_API_FUNCTION(int, ompt_get_parallel_info, (
    int ancestor_level,
    ompt_data_t **parallel_data,
    int *team_size
));

/* task */
OMPT_API_FUNCTION(int, ompt_get_task_info, (
    int ancestor_level,
    int *type,
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

OMPT_API_FUNCTION(int, ompt_initialize, (
    ompt_function_lookup_t ompt_fn_lookup,
    ompt_fns_t *fns
));

OMPT_API_FUNCTION(void, ompt_finalize, (
    ompt_fns_t *fns
));

typedef struct ompt_fns_t {
    ompt_initialize_t initialize;
    ompt_finalize_t finalize;
} ompt_fns_t;

/* initialization interface to be defined by tool */
ompt_fns_t * ompt_start_tool(
    unsigned int omp_version, 
    const char * runtime_version
);

typedef enum opt_init_mode_e {
    ompt_init_mode_never  = 0,
    ompt_init_mode_false  = 1,
    ompt_init_mode_true   = 2,
    ompt_init_mode_always = 3
} ompt_init_mode_t;

OMPT_API_FUNCTION(int, ompt_set_callback, (
    ompt_callbacks_t which,
    ompt_callback_t callback
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
// FIXME: remove workaround for clang
#if !defined(__clang__) && defined(_OPENMP) && (_OPENMP >= 201307)
#pragma omp declare target
#endif
void ompt_control(
    uint64_t command,
    uint64_t modifier
);
#if !defined(__clang__) && defined(_OPENMP) && (_OPENMP >= 201307)
#pragma omp end declare target
#endif

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

/* get_unique_id */
OMPT_API_FUNCTION(uint64_t, ompt_get_unique_id, (void));

#ifdef  __cplusplus
};
#endif

#endif
