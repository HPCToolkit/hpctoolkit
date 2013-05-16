#ifndef task_cntxt_h
#define task_cntxt_h

#include "frame.h"
/*
 * this interface is used for lazy callpath resolution 
*/

/* register the functions in runtime (OMP) to support 
   task context resolution */
void register_task_callback();

/* return the task creation context (or NULL) */
void* need_task_cntxt();

void* copy_task_cntxt(void *);

void hack_task_context(frame_t **, frame_t **);
#endif
