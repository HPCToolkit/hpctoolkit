/*
 * include/45/ompt.h.var
 */

#ifndef __OMPT__
#define __OMPT__

/*****************************************************************************
 * system include files
 *****************************************************************************/

#include <stdint.h>
#include <unistd.h>


/*****************************************************************************
 * iteration macros
 *****************************************************************************/

#define FOREACH_OMPT_INQUIRY_FN(macro)  \
    macro (ompt_enumerate_state)        \
                                        \
    macro (ompt_set_callback)           \
    macro (ompt_get_callback)           \
                                        \
    macro (ompt_get_idle_frame)         \
    macro (ompt_get_task_frame)         \
                                        \
    macro (ompt_get_state)              \
                                        \
    macro (ompt_get_parallel_id)        \
    macro (ompt_get_parallel_team_size) \
    macro (ompt_get_task_id)            \
    macro (ompt_get_thread_id)     

#define FOREACH_OMPT_PLACEHOLDER_FN(macro)  \
    macro (ompt_idle)                       \
    macro (ompt_overhead)                   \
    macro (ompt_barrier_wait)               \
    macro (ompt_task_wait)                  \
    macro (ompt_mutex_wait)

#define FOREACH_OMPT_STATE(macro)                                                               \
                                                                                                \
    /* first */                                                                                 \
    macro (ompt_state_first, 0x71)          /* initial enumeration state */                     \
                                                                                                \
    /* work states (0..15) */                                                                   \
    macro (ompt_state_work_serial, 0x00)    /* working outside parallel */                      \
    macro (ompt_state_work_parallel, 0x01)  /* working within parallel */                       \
    macro (ompt_state_work_reduction, 0x02) /* performing a reduction */                        \
                                                                                                \
    /* idle (16..31) */                                                                         \
    macro (ompt_state_idle, 0x10)            /* waiting for work */                             \
                                                                                                \
    /* overhead states (32..63) */                                                              \
    macro (ompt_state_overhead, 0x20)        /* overhead excluding wait states */               \
                                                                                                \
    /* barrier wait states (64..79) */                                                          \
    macro (ompt_state_wait_barrier, 0x40)    /* waiting at a barrier */                         \
    macro (ompt_state_wait_barrier_implicit, 0x41)    /* implicit barrier */                    \
    macro (ompt_state_wait_barrier_explicit, 0x42)    /* explicit barrier */                    \
                                                                                                \
    /* task wait states (80..95) */                                                             \
    macro (ompt_state_wait_taskwait, 0x50)   /* waiting at a taskwait */                        \
    macro (ompt_state_wait_taskgroup, 0x51)  /* waiting at a taskgroup */                       \
                                                                                                \
    /* mutex wait states (96..111) */                                                           \
    macro (ompt_state_wait_lock, 0x60)       /* waiting for lock */                             \
    macro (ompt_state_wait_nest_lock, 0x61)  /* waiting for nest lock */                        \
    macro (ompt_state_wait_critical, 0x62)   /* waiting for critical */                         \
    macro (ompt_state_wait_atomic, 0x63)     /* waiting for atomic */                           \
    macro (ompt_state_wait_ordered, 0x64)    /* waiting for ordered */                          \
    macro (ompt_state_wait_single, 0x6F)     /* waiting for single region (non-standard!) */    \
                                                                                                \
    /* misc (112..127) */                                                                       \
    macro (ompt_state_undefined, 0x70)       /* undefined thread state */


#define FOREACH_OMPT_EVENT(macro)                                                                               \
                                                                                                                \
    /*--- Mandatory Events ---*/                                                                                \
    macro (ompt_event_parallel_begin,           ompt_new_parallel_callback_t,   1) /* parallel begin */         \
    macro (ompt_event_parallel_end,             ompt_end_parallel_callback_t,   2) /* parallel end */           \
                                                                                                                \
    macro (ompt_event_task_begin,               ompt_new_task_callback_t,       3) /* task begin */             \
    macro (ompt_event_task_end,                 ompt_task_callback_t,           4) /* task destroy */           \
                                                                                                                \
    macro (ompt_event_thread_begin,             ompt_thread_type_callback_t,    5) /* thread begin */           \
    macro (ompt_event_thread_end,               ompt_thread_type_callback_t,    6) /* thread end */             \
                                                                                                                \
    macro (ompt_event_control,                  ompt_control_callback_t,        7) /* support control calls */  \
                                                                                                                \
    macro (ompt_event_runtime_shutdown,         ompt_callback_t,                8) /* runtime shutdown */       \
                                                                                                                \
    /*--- Optional Events (blame shifting, ompt_event_unimplemented) ---*/                                      \
    macro (ompt_event_idle_begin,               ompt_thread_callback_t,         9) /* begin idle state */       \
    macro (ompt_event_idle_end,                 ompt_thread_callback_t,        10) /* end idle state */         \
                                                                                                                \
    macro (ompt_event_wait_barrier_begin,       ompt_parallel_callback_t,      11) /* begin wait at barrier */  \
    macro (ompt_event_wait_barrier_end,         ompt_parallel_callback_t,      12) /* end wait at barrier */    \
                                                                                                                \
    macro (ompt_event_wait_taskwait_begin,      ompt_parallel_callback_t,      13) /* begin wait at taskwait */ \
    macro (ompt_event_wait_taskwait_end,        ompt_parallel_callback_t,      14) /* end wait at taskwait */   \
                                                                                                                \
    macro (ompt_event_wait_taskgroup_begin,     ompt_parallel_callback_t,      15) /* begin wait at taskgroup */\
    macro (ompt_event_wait_taskgroup_end,       ompt_parallel_callback_t,      16) /* end wait at taskgroup */  \
                                                                                                                \
    macro (ompt_event_release_lock,             ompt_wait_callback_t,          17) /* lock release */           \
    macro (ompt_event_release_nest_lock_last,   ompt_wait_callback_t,          18) /* last nest lock release */ \
    macro (ompt_event_release_critical,         ompt_wait_callback_t,          19) /* critical release */       \
                                                                                                                \
    macro (ompt_event_release_atomic,           ompt_wait_callback_t,          20) /* atomic release */         \
                                                                                                                \
    macro (ompt_event_release_ordered,          ompt_wait_callback_t,          21) /* ordered release */        \
                                                                                                                \
    /*--- Optional Events (synchronous events, ompt_event_unimplemented) --- */                                 \
    macro (ompt_event_implicit_task_begin,      ompt_parallel_callback_t,      22) /* implicit task begin   */  \
    macro (ompt_event_implicit_task_end,        ompt_parallel_callback_t,      23) /* implicit task end  */     \
                                                                                                                \
    macro (ompt_event_initial_task_begin,       ompt_parallel_callback_t,      24) /* initial task begin   */   \
    macro (ompt_event_initial_task_end,         ompt_parallel_callback_t,      25) /* initial task end  */      \
                                                                                                                \
    macro (ompt_event_task_switch,              ompt_task_pair_callback_t,     26) /* task switch */            \
                                                                                                                \
    macro (ompt_event_loop_begin,               ompt_new_workshare_callback_t, 27) /* task at loop begin */     \
    macro (ompt_event_loop_end,                 ompt_parallel_callback_t,      28) /* task at loop end */       \
                                                                                                                \
    macro (ompt_event_sections_begin,           ompt_new_workshare_callback_t, 29) /* task at sections begin  */\
    macro (ompt_event_sections_end,             ompt_parallel_callback_t,      30) /* task at sections end */   \
                                                                                                                \
    macro (ompt_event_single_in_block_begin,    ompt_new_workshare_callback_t, 31) /* task at single begin*/    \
    macro (ompt_event_single_in_block_end,      ompt_parallel_callback_t,      32) /* task at single end */     \
                                                                                                                \
    macro (ompt_event_single_others_begin,      ompt_parallel_callback_t,      33) /* task at single begin */   \
    macro (ompt_event_single_others_end,        ompt_parallel_callback_t,      34) /* task at single end */     \
                                                                                                                \
    macro (ompt_event_workshare_begin,          ompt_new_workshare_callback_t, 35) /* task at workshare begin */\
    macro (ompt_event_workshare_end,            ompt_parallel_callback_t,      36) /* task at workshare end */  \
                                                                                                                \
    macro (ompt_event_master_begin,             ompt_parallel_callback_t,      37) /* task at master begin */   \
    macro (ompt_event_master_end,               ompt_parallel_callback_t,      38) /* task at master end */     \
                                                                                                                \
    macro (ompt_event_barrier_begin,            ompt_parallel_callback_t,      39) /* task at barrier begin  */ \
    macro (ompt_event_barrier_end,              ompt_parallel_callback_t,      40) /* task at barrier end */    \
                                                                                                                \
    macro (ompt_event_taskwait_begin,           ompt_parallel_callback_t,      41) /* task at taskwait begin */ \
    macro (ompt_event_taskwait_end,             ompt_parallel_callback_t,      42) /* task at task wait end */  \
                                                                                                                \
    macro (ompt_event_taskgroup_begin,          ompt_parallel_callback_t,      43) /* task at taskgroup begin */\
    macro (ompt_event_taskgroup_end,            ompt_parallel_callback_t,      44) /* task at taskgroup end */  \
                                                                                                                \
    macro (ompt_event_release_nest_lock_prev,   ompt_wait_callback_t,          45) /* prev nest lock release */ \
                                                                                                                \
    macro (ompt_event_wait_lock,                ompt_wait_callback_t,          46) /* lock wait */              \
    macro (ompt_event_wait_nest_lock,           ompt_wait_callback_t,          47) /* nest lock wait */         \
    macro (ompt_event_wait_critical,            ompt_wait_callback_t,          48) /* critical wait */          \
    macro (ompt_event_wait_atomic,              ompt_wait_callback_t,          49) /* atomic wait */            \
    macro (ompt_event_wait_ordered,             ompt_wait_callback_t,          50) /* ordered wait */           \
                                                                                                                \
    macro (ompt_event_acquired_lock,            ompt_wait_callback_t,          51) /* lock acquired */          \
    macro (ompt_event_acquired_nest_lock_first, ompt_wait_callback_t,          52) /* 1st nest lock acquired */ \
    macro (ompt_event_acquired_nest_lock_next,  ompt_wait_callback_t,          53) /* next nest lock acquired*/ \
    macro (ompt_event_acquired_critical,        ompt_wait_callback_t,          54) /* critical acquired */      \
    macro (ompt_event_acquired_atomic,          ompt_wait_callback_t,          55) /* atomic acquired */        \
    macro (ompt_event_acquired_ordered,         ompt_wait_callback_t,          56) /* ordered acquired */       \
                                                                                                                \
    macro (ompt_event_init_lock,                ompt_wait_callback_t,          57) /* lock init */              \
    macro (ompt_event_init_nest_lock,           ompt_wait_callback_t,          58) /* nest lock init */         \
                                                                                                                \
    macro (ompt_event_destroy_lock,             ompt_wait_callback_t,          59) /* lock destruction */       \
    macro (ompt_event_destroy_nest_lock,        ompt_wait_callback_t,          60) /* nest lock destruction */  \
                                                                                                                \
    macro (ompt_event_flush,                    ompt_callback_t,               61) /* after executing flush */  \
                                                                                                                \
    macro (ompt_event_task_dependences,         ompt_task_dependences_callback_t, 69) /* report task dependences  */\
    macro (ompt_event_task_dependence_pair,     ompt_task_pair_callback_t,     70) /* report task dependence pair */\
    macro (ompt_callback_target,                ompt_callback_target_t,            71) /* target region*/\
    macro (ompt_callback_device_initialize,	ompt_callback_device_initialize_t, 72) /* initialize device tracing interface */\
    macro (ompt_callback_device_finalize,	ompt_callback_device_finalize_t,   73) /* finalize device tracing interface */\
    macro (ompt_callback_device_load,		ompt_callback_device_load_t,       74) /* load device code       */\
    macro (ompt_callback_device_unload,		ompt_callback_device_unload_t,     75) /* unload device code     */\
    macro (ompt_callback_target_submit,		ompt_callback_target_submit_t,     76) /* submit device kernel   */\
    macro (ompt_callback_target_data_op,	ompt_callback_target_data_op_t,    77) /* device data operation  */\
    macro (ompt_callback_target_map,		ompt_callback_target_map_t,        78) /* device data mapping op */

#define FOREACH_OMPT_TARGET_CALLBACK(macro)					   \
  macro(ompt_callback_device_initialize)					   \
  macro(ompt_callback_device_finalize)						   \
  macro(ompt_callback_device_load)						   \
  macro(ompt_callback_device_unload)						   \
  macro(ompt_callback_target)							   \
  macro(ompt_callback_target_map)						   \
  macro(ompt_callback_target_data_op)						   \
  macro(ompt_callback_target_submit)



/*****************************************************************************
 * data types
 *****************************************************************************/

/*---------------------
 * identifiers
 *---------------------*/

typedef uint64_t ompt_thread_id_t;
#define ompt_thread_id_none ((ompt_thread_id_t) 0)     /* non-standard */

typedef uint64_t ompt_task_id_t;
#define ompt_task_id_none ((ompt_task_id_t) 0)         /* non-standard */

typedef uint64_t ompt_parallel_id_t;
#define ompt_parallel_id_none ((ompt_parallel_id_t) 0) /* non-standard */

typedef uint64_t ompt_wait_id_t;
#define ompt_wait_id_none ((ompt_wait_id_t) 0)         /* non-standard */

typedef union ompt_data_u {
  uint64_t value;
  void *ptr;
} ompt_data_t;


static const ompt_data_t ompt_data_none = {0};


/*---------------------
 * ompt_frame_t
 *---------------------*/

typedef struct ompt_frame_s {
    void *exit_runtime_frame;    /* next frame is user code     */
    void *reenter_runtime_frame; /* previous frame is user code */
} ompt_frame_t;


/*---------------------
 * registration return codes
 *---------------------*/

typedef enum opt_init_mode_e {
    ompt_init_mode_never  = 0,
    ompt_init_mode_false  = 1,
    ompt_init_mode_true   = 2,
    ompt_init_mode_always = 3
} ompt_init_mode_t;


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

/*---------------------
 * callback support 
 *---------------------*/

typedef enum ompt_scope_endpoint_e{
    ompt_scope_begin    = 1,
    ompt_scope_end      = 2
} ompt_scope_endpoint_t;

/*---------------------
 * target types
 *---------------------*/

typedef int32_t ompt_device_t; 

typedef uint64_t ompt_device_time_t; 
static const ompt_device_time_t ompt_device_time_none = 0;

typedef void ompt_buffer_t;
typedef uint64_t ompt_buffer_cursor_t; 

typedef uint64_t ompt_target_id_t; 

typedef enum ompt_target_type_e {
    ompt_task_target	        = 1,
    ompt_task_target_enter_data = 2,
    ompt_task_target_exit_data  = 3,
    ompt_task_target_update	= 4
} ompt_target_type_t;

typedef enum ompt_target_data_op_e {
  ompt_target_data_alloc              = 1,
  ompt_target_data_transfer_to_dev    = 2,
  ompt_target_data_transfer_from_dev  = 3,
  ompt_target_data_delete             = 4
} ompt_target_data_op_t;

typedef enum ompt_native_mon_flags_e {
    ompt_native_data_motion_explicit = 1,
    ompt_native_data_motion_implicit = 2,
    ompt_native_kernel_invocation    = 4,
    ompt_native_kernel_execution     = 8,
    ompt_native_driver               = 16,
    ompt_native_runtime	             = 32,
    ompt_native_overhead	     = 64,
    ompt_native_idleness	     = 128
} ompt_native_mon_flags_t;

typedef enum ompt_record_type_e {
    ompt_record_ompt    = 1,
    ompt_record_native  = 2,
    ompt_record_invalid = 3
} ompt_record_type_t; 

typedef enum ompt_record_native_kind_e {
  ompt_record_native_info  = 1,
  ompt_record_native_event = 2
} ompt_record_native_kind_t;

typedef uint64_t ompt_id_t;
#define ompt_id_none 0

typedef uint64_t ompt_hwid_t; 
#define ompt_hwid_none 0

typedef struct ompt_record_abstract_s { 
    ompt_record_native_kind_t rclass; 
    const char *type;
    ompt_device_time_t start_time; 
    ompt_device_time_t end_time; 
    ompt_hwid_t hwid;
} ompt_record_abstract_t;

typedef struct ompt_record_type_s {
#if 0
  ompt_record_thread_begin_t thread_begin;
  ompt_record_idle_t idle; 
  ompt_record_parallel_begin_t parallel_begin;
  ompt_record_parallel_end_t parallel_end;
  ompt_record_task_create_t task_create;
  ompt_record_task_dependence_t task_dep;
  ompt_record_task_schedule_t task_sched;
  ompt_record_implicit_t implicit;
  ompt_record_sync_region_t sync_region;
  ompt_record_target_t target_record;
  ompt_record_target_data_op_t target_data_op;
  ompt_record_target_map_t target_map;
  ompt_record_target_kernel_t kernel;
  ompt_record_lock_init_t lock_init;
  ompt_record_lock_destroy_t lock_destroy;
  ompt_record_mutex_acquire_t mutex_acquire;
  ompt_record_mutex_t mutex;
  ompt_record_nest_lock_t nest_lock;
  ompt_record_master_t master;
  ompt_record_work_t work;
  ompt_record_flush_t flush;
#else
  int FIXME;
#endif
} ompt_record_ompt_t;



/*****************************************************************************
 * enumerations for thread states and runtime events
 *****************************************************************************/

/*---------------------
 * runtime states
 *---------------------*/

typedef enum {
#define ompt_state_macro(state, code) state = code,
    FOREACH_OMPT_STATE(ompt_state_macro)
#undef ompt_state_macro
} ompt_state_t;


/*---------------------
 * runtime events
 *---------------------*/

typedef enum {
#define ompt_event_macro(event, callback, eventid) event = eventid,
    FOREACH_OMPT_EVENT(ompt_event_macro)
#undef ompt_event_macro
} ompt_event_t;


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
    const char *interface_fn_name     /* entry point to look up       */
);

/* threads */
typedef void (*ompt_thread_callback_t) (
    ompt_thread_id_t thread_id        /* ID of thread                 */
);

typedef enum {
    ompt_thread_initial = 1, // start the enumeration at 1
    ompt_thread_worker  = 2,
    ompt_thread_other   = 3,
    ompt_thread_unknown = 4
} ompt_thread_type_t;

typedef enum {
    ompt_invoker_program = 0,         /* program invokes master task  */
    ompt_invoker_runtime = 1          /* runtime invokes master task  */
} ompt_invoker_t;

typedef void (*ompt_thread_type_callback_t) (
    ompt_thread_type_t thread_type,   /* type of thread               */
    ompt_thread_id_t thread_id        /* ID of thread                 */
);

typedef void (*ompt_wait_callback_t) (
    ompt_wait_id_t wait_id            /* wait id                      */
);

/* parallel and workshares */
typedef void (*ompt_parallel_callback_t) (
    ompt_parallel_id_t parallel_id,    /* id of parallel region       */
    ompt_task_id_t task_id             /* id of task                  */
);

typedef void (*ompt_new_workshare_callback_t) (
    ompt_parallel_id_t parallel_id,   /* id of parallel region        */
    ompt_task_id_t parent_task_id,    /* id of parent task            */
    void *workshare_function          /* pointer to outlined function */
);

typedef void (*ompt_new_parallel_callback_t) (
    ompt_task_id_t parent_task_id,    /* id of parent task            */
    ompt_frame_t *parent_task_frame,  /* frame data of parent task    */
    ompt_parallel_id_t parallel_id,   /* id of parallel region        */
    uint32_t requested_team_size,     /* number of threads in team    */
    void *parallel_function,          /* pointer to outlined function */
    ompt_invoker_t invoker            /* who invokes master task?     */
);

typedef void (*ompt_end_parallel_callback_t) (
    ompt_parallel_id_t parallel_id,   /* id of parallel region       */
    ompt_task_id_t task_id,           /* id of task                  */
    ompt_invoker_t invoker            /* who invokes master task?    */
);

/* tasks */
typedef void (*ompt_task_callback_t) (
    ompt_task_id_t task_id            /* id of task                   */
);

typedef void (*ompt_task_pair_callback_t) (
    ompt_task_id_t first_task_id,
    ompt_task_id_t second_task_id
);

typedef void (*ompt_new_task_callback_t) (
    ompt_task_id_t parent_task_id,    /* id of parent task            */
    ompt_frame_t *parent_task_frame,  /* frame data for parent task   */
    ompt_task_id_t  new_task_id,      /* id of created task           */
    void *task_function               /* pointer to outlined function */
);

/* task dependences */
typedef void (*ompt_task_dependences_callback_t) (
    ompt_task_id_t task_id,            /* ID of task with dependences */
    const ompt_task_dependence_t *deps,/* vector of task dependences  */
    int ndeps                          /* number of dependences       */
);

/* program */
typedef void (*ompt_control_callback_t) (
    uint64_t command,                 /* command of control call      */
    uint64_t modifier                 /* modifier of control call     */
);

#define ompt_value_unknown (~0ULL) 

/* target */ 
typedef void (*ompt_callback_device_initialize_t)(
    uint64_t device_num,
    const char *type,
    ompt_device_t *device,
    ompt_function_lookup_t lookup,
    const char *documentation
);

typedef void (*ompt_callback_device_finalize_t)(
    uint64_t device_num
);

typedef void (*ompt_callback_device_load_t)(
    uint64_t device_num,
    const char *filename,
    int64_t file_offset,
    const void *file_addr,
    size_t bytes,
    const void *host_addr,
    const void *device_addr,
    uint64_t module_id
);

typedef void (*ompt_callback_device_unload_t)
(
    uint64_t device_num,
    uint64_t module
);

typedef void (*ompt_callback_target_t)
(
    ompt_target_type_t kind,
    ompt_scope_endpoint_t endpoint,
    uint64_t device_num,
    ompt_data_t *task_data,
    ompt_target_id_t target_id,
    const void *codeptr_ra
);

typedef void (*ompt_callback_target_submit_t)
(
    ompt_id_t target_id,
    ompt_id_t host_op_id
);

typedef void (*ompt_callback_target_data_op_t)
( 
    ompt_id_t target_id,
    ompt_id_t host_op_id,
    ompt_target_data_op_t optype,
    void *host_addr, 
    void *device_addr, 
    size_t bytes
);

typedef void (*ompt_callback_target_map_t)
( 
    ompt_id_t target_id,
    unsigned int nitems,
    void **host_addr,
    void **device_addr,
    size_t *bytes,
    unsigned int *mapping_flags
);

typedef void (*ompt_callback_t)(void);


/****************************************************************************
 * ompt API
 ***************************************************************************/

//#define __cplusplus

#ifdef  __cplusplus
extern "C" {
#endif

#define OMPT_API_FNTYPE(fn) fn##_t

#define OMPT_API_FUNCTION(return_type, fn, args)  \
    typedef return_type (*OMPT_API_FNTYPE(fn)) args

#define OMPT_TARGET_API_FUNCTION(return_type, fn, args)  \
    OMPT_API_FUNCTION(return_type, fn, args) 


/****************************************************************************
 * INQUIRY FUNCTIONS
 ***************************************************************************/

/* state */
OMPT_API_FUNCTION(ompt_state_t, ompt_get_state, (
    ompt_wait_id_t *ompt_wait_id
));

/* thread */
OMPT_API_FUNCTION(ompt_thread_id_t, ompt_get_thread_id, (void));

OMPT_API_FUNCTION(void *, ompt_get_idle_frame, (void));

/* parallel region */
OMPT_API_FUNCTION(ompt_parallel_id_t, ompt_get_parallel_id, (
    int ancestor_level
));

OMPT_API_FUNCTION(int, ompt_get_parallel_team_size, (
    int ancestor_level
));

/* task */
OMPT_API_FUNCTION(ompt_task_id_t, ompt_get_task_id, (
    int depth
));

OMPT_API_FUNCTION(ompt_frame_t *, ompt_get_task_frame, (
    int depth
));



/****************************************************************************
 * TARGET TOOL CALLBACKS
 ***************************************************************************/

OMPT_TARGET_API_FUNCTION(void, ompt_callback_buffer_request, (
    uint64_t device_num,
    ompt_buffer_t **buffer,
    size_t *bytes
));

OMPT_TARGET_API_FUNCTION(void, ompt_callback_buffer_complete, (
    uint64_t device_num,
    ompt_buffer_t *buffer,
    size_t bytes,
    ompt_buffer_cursor_t begin,
    int buffer_owned
));

/****************************************************************************
 * TARGET CONTROL API
 ***************************************************************************/


OMPT_TARGET_API_FUNCTION(ompt_device_time_t, ompt_get_device_time, (
    ompt_device_t *device
));

OMPT_TARGET_API_FUNCTION(double, ompt_translate_time, (
    ompt_device_t *device,
    ompt_device_time_t time
));

OMPT_TARGET_API_FUNCTION(int, ompt_set_trace_ompt, (
    ompt_device_t *device,
    unsigned int enable,
    unsigned int flags
));

OMPT_TARGET_API_FUNCTION(int, ompt_set_trace_native, (
    ompt_device_t *device,
    int enable,
    int flags
));

OMPT_TARGET_API_FUNCTION(int, ompt_start_trace, (
    ompt_device_t *device,
    ompt_callback_buffer_request_t request,
    ompt_callback_buffer_complete_t complete
));

OMPT_TARGET_API_FUNCTION(int, ompt_pause_trace, (
    ompt_device_t *device,
    int begin_pause
));

OMPT_TARGET_API_FUNCTION(int, ompt_stop_trace, (
    ompt_device_t *device
));

OMPT_TARGET_API_FUNCTION(int, ompt_advance_buffer_cursor, (
    ompt_buffer_t *buffer,
    size_t size,
    ompt_buffer_cursor_t current,
    ompt_buffer_cursor_t *next
));

OMPT_TARGET_API_FUNCTION(ompt_record_type_t, ompt_get_record_type, (
    ompt_buffer_t *buffer,
    ompt_buffer_cursor_t current
));
	
OMPT_TARGET_API_FUNCTION(void *, ompt_get_record_native, (
    ompt_buffer_t *buffer,
    ompt_buffer_cursor_t current,
    ompt_id_t *host_op_id
));
	
OMPT_TARGET_API_FUNCTION(ompt_record_ompt_t *, ompt_get_record_ompt, (
    ompt_buffer_t *buffer,
    ompt_buffer_cursor_t current,
    ompt_id_t *host_op_id
));

OMPT_TARGET_API_FUNCTION(ompt_record_abstract_t *, ompt_get_record_abstract, (
    void *native_record
));



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

OMPT_API_FUNCTION(void, ompt_initialize, (
    ompt_function_lookup_t ompt_fn_lookup,
    const char *runtime_version,
    unsigned int ompt_version
));

OMPT_API_FUNCTION(void, ompt_initialize_5, (
    ompt_function_lookup_t ompt_fn_lookup,
    struct ompt_fns_t *fns
));

OMPT_API_FUNCTION(void, ompt_finalize, (
    struct ompt_fns_t *fns
));

typedef struct ompt_fns_t {
  ompt_initialize_5_t initialize;
  ompt_finalize_t   finalize;
} ompt_fns_t;


/* initialization interface to be defined by tool */
ompt_initialize_t ompt_tool(void);


/* initialization interface used by libomptarget */
void libomp_libomptarget_ompt_init(ompt_fns_t *);


OMPT_API_FUNCTION(int, ompt_set_callback, (
    ompt_event_t event,
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
    ompt_event_t event,
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
OMPT_API_FUNCTION(int, ompt_enumerate_state, (
    int current_state,
    int *next_state,
    const char **next_state_name
));


#ifdef  __cplusplus
}
#endif

#endif

