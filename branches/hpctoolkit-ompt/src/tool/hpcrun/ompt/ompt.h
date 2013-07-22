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


/*---------------------
 * identifiers
 *---------------------*/

typedef uint64_t ompt_parallel_id_t;
typedef uint64_t ompt_wait_id_t;


/*---------------------
 * ompt_data_t
 *---------------------*/

typedef union ompt_data_u {
  uint64_t value;               /* data under tool control    */
  void *ptr;                    /* pointer under tool control */
} ompt_data_t;


/*---------------------
 * ompt_frame_t
 *---------------------*/

typedef struct ompt_frame_s {
   void *exit_runtime_frame;    /* next frame is user code     */
   void *reenter_runtime_frame; /* previous frame is user code */
} ompt_frame_t;


/*---------------------
 * callback interfaces
 *---------------------*/

typedef void (*ompt_thread_callback_t) (
  ompt_data_t *thread_data           /* tool data for thread       */
  );

typedef void (*ompt_parallel_callback_t) (
  ompt_data_t *task_data,           /* tool data for a task        */
  ompt_parallel_id_t parallel_id    /* id of parallel region       */
  );

typedef void (*ompt_new_parallel_callback_t) (
  ompt_data_t  *parent_task_data,   /* tool data for parent task   */
  ompt_frame_t *parent_task_frame,  /* frame data of parent task   */
  ompt_parallel_id_t parallel_id    /* id of parallel region       */
  );

typedef void (*ompt_task_callback_t) (
  ompt_data_t *task_data            /* tool data for task          */
  );

typedef void (*ompt_task_switch_callback_t) (
  ompt_data_t *suspended_task_data, /* tool data for suspended task */
  ompt_data_t *resumed_task_data    /* tool data for resumed task   */
  );

typedef void (*ompt_new_task_callback_t) (
  ompt_data_t  *parent_task_data,   /* tool data for parent task    */
  ompt_frame_t *parent_task_frame,  /* frame data for parent task   */
  ompt_data_t  *new_task_data       /* tool data for created task   */
  );

typedef void (*ompt_wait_callback_t) (
  ompt_wait_id_t wait_id            /* wait id                      */
  );

typedef void (*ompt_control_callback_t) (
  uint64_t command,                /* command of control call      */
  uint64_t modifier                /* modifier of control call     */
  );

typedef void (*ompt_callback_t) (
  );



/****************************************************************************
 * public variables 
 ***************************************************************************/

/* debugger interface */
extern char **ompd_dll_locations; 


/****************************************************************************
 * ompt API 
 ***************************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif 

/* initialization */
extern int 			ompt_initialize(void);
extern int 			ompt_set_callback(ompt_event_t evid, ompt_callback_t cb);
extern int 			ompt_get_callback(ompt_event_t evid, ompt_callback_t *cb);

/* state */
extern ompt_state_t 		ompt_get_state(ompt_wait_id_t *ompt_wait_id);

/* thread */
extern ompt_data_t *		ompt_get_thread_data(void);

/* thread */
extern void *		        ompt_get_thread_idle_frame(void);


/* parallel region */
extern ompt_parallel_id_t 	ompt_get_parallel_id(int ancestor_level);
extern void *			ompt_get_parallel_function(int ancestor_level);

/* task */
extern ompt_data_t *		ompt_get_task_data(int ancestor_level);
extern ompt_frame_t *		ompt_get_task_frame(int ancestor_level);
extern void *			ompt_get_task_function(int ancestor_level);

/* library inquiry */
extern int 			ompt_get_runtime_version(char *buffer, int length);
extern int 			ompt_enumerate_state(int current_state, int *next_state, const char **next_state_name);
extern int 			ompt_get_ompt_version(void);

/* control */
extern void 			ompt_control(uint64_t command, uint64_t modifier);

#ifdef  __cplusplus
};
#endif 




#endif

