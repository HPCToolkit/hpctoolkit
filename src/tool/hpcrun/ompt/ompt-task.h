#ifndef __ompt_task_h__
#define __ompt_task_h__

#include <ompt.h>


/******************************************************************************
 * global data
 *****************************************************************************/

int ompt_task_full_context;

/******************************************************************************
 * interface operations
 *****************************************************************************/

void ompt_task_register_callbacks(ompt_set_callback_t ompt_set_callback_fn);

#endif
