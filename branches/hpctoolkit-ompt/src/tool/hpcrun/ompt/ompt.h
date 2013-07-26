#ifndef __OMPT___
#define __OMPT___

/*****************************************************************************
 * system include files
 *****************************************************************************/

#include <stdint.h>


/*****************************************************************************
 * data types
 *****************************************************************************/

/*---------------------
 * identifiers
 *---------------------*/

typedef uint64_t ompt_task_id_t;
#define ompt_task_id_none ((ompt_task_id_t) 0) /* non-standard */

typedef uint64_t ompt_parallel_id_t;
#define ompt_parallel_id_none ((ompt_parallel_id_t) 0) /* non-standard */

typedef uint64_t ompt_wait_id_t;
#define ompt_wait_id_none ((ompt_wait_id_t) 0) /* non-standard */


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
#define ompt_state(state, code) state = code,
#include "ompt-state.h"
} ompt_state_t;


/*---------------------
 * runtime events
 *---------------------*/

typedef enum {
#define ompt_event(event, callback, eventid, is_impl) event = eventid,
#include "ompt-event.h"
} ompt_event_t;



/*****************************************************************************
 * callback signatures
 *****************************************************************************/

/* threads */
typedef void (*ompt_callback_t)(void);

typedef void (*ompt_wait_callback_t) (
  ompt_wait_id_t wait_id            /* wait id                      */
  );


/* parallel and workshares */
typedef void (*ompt_parallel_callback_t) (
  ompt_parallel_id_t parallel_id,    /* id of parallel region       */
  ompt_task_id_t task_id             /* id of task                  */
  );


typedef void (*ompt_new_workshare_callback_t) (
  ompt_task_id_t parent_task_id,    /* id of parent task            */
  ompt_parallel_id_t parallel_id,   /* id of parallel region        */
  void *workshare_function          /* pointer to outlined function */
  );

typedef void (*ompt_new_parallel_callback_t) (
  ompt_task_id_t parent_task_id,    /* id of parent task            */
  ompt_frame_t *parent_task_frame,  /* frame data of parent task    */
  ompt_parallel_id_t parallel_id,   /* id of parallel region        */
  void *parallel_function           /* pointer to outlined function */
  );


/* tasks */
typedef void (*ompt_task_callback_t) (
  ompt_task_id_t task_id            /* id of task                   */
  );

typedef void (*ompt_task_switch_callback_t) (
  ompt_task_id_t suspended_task_id, /* tool data for suspended task */
  ompt_task_id_t resumed_task_id    /* tool data for resumed task   */
  );

typedef void (*ompt_new_task_callback_t) (
  ompt_task_id_t parent_task_id,    /* id of parent task            */
  ompt_frame_t *parent_task_frame,  /* frame data for parent task   */
  ompt_task_id_t  new_task_id,      /* id of created task           */
  void *task_function               /* pointer to outlined function */
  );


/* program */
typedef void (*ompt_control_callback_t) (
  uint64_t command,                /* command of control call      */
  uint64_t modifier                /* modifier of control call     */
  );



/****************************************************************************
 * ompt API 
 ***************************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif 



/****************************************************************************
 * INQUIRY FUNCTIONS
 ***************************************************************************/

/* state */
extern ompt_state_t ompt_get_state(
  ompt_wait_id_t *ompt_wait_id
);

/* thread */
extern void *ompt_get_idle_frame(void);

/* parallel region */
extern ompt_parallel_id_t ompt_get_parallel_id(
  int ancestor_level
);

/* task */
extern ompt_task_id_t ompt_get_task_id(
  int ancestor_level
);

extern ompt_frame_t *ompt_get_task_frame(
  int ancestor_level
);


/****************************************************************************
 * INITIALIZATION FUNCTIONS
 ***************************************************************************/

extern int ompt_initialize(void); /* to be defined by tool */

typedef enum opt_init_mode_e {
  ompt_init_mode_never  = 0,
  ompt_init_mode_false  = 1,
  ompt_init_mode_true   = 2,
  ompt_init_mode_always = 3
} ompt_init_mode_t;

extern int ompt_set_callback(
  ompt_event_t event, 
  ompt_callback_t callback
);

typedef enum ompt_set_callback_rc_e {  /* non-standard */
  ompt_set_callback_error      = 0,
  ompt_has_event_no_callback   = 1,
  ompt_no_event_no_callback    = 2,
  ompt_has_event_may_callback  = 3,
  ompt_has_event_must_callback = 4,
} ompt_set_callback_rc_t;


extern int ompt_get_callback(
  ompt_event_t event, 
  ompt_callback_t *callback
);

/****************************************************************************
 * MISCELLANEOUS FUNCTIONS
 ***************************************************************************/

/* control */
extern void ompt_control(
  uint64_t command, 
  uint64_t modifier);

/* library inquiry */
extern int ompt_get_ompt_version(void);

extern int ompt_get_runtime_version(
  char *buffer, 
  int length
);

extern int ompt_enumerate_state(
  int current_state, 
  int *next_state, 
  const char **next_state_name
);


#ifdef  __cplusplus
};
#endif


/****************************************************************************
 * public variables 
 ***************************************************************************/

/* debugger interface */
extern char **ompd_dll_locations; 



#endif

