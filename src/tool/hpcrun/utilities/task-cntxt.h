#ifndef task_cntxt_h
#define task_cntxt_h

#include <ompt.h>

#include "frame.h"
/*
 * this interface is used for lazy callpath resolution 
*/

/* return the task creation context (or NULL) */
void* need_task_cntxt();

/* collect full task context, using eager unwinds where needed */
void task_cntxt_full();

void* copy_task_cntxt(void *);

void hack_task_context(frame_t **, frame_t **);

// export registration interfaces for ompt
void start_task_fn(ompt_data_t *parent_task_data, 
		   ompt_frame_t *parent_task_frame, 
		   ompt_data_t *new_task_data);
#endif
