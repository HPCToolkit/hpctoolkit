#ifndef _IDLE_H_
#define _IDLE_H_

#include <ompt.h>

void idle_fn(ompt_data_t *thread_data);
void work_fn(ompt_data_t *thread_data);
void start_fn(ompt_data_t *thread_data);
void end_fn(ompt_data_t *thread_data);
void bar_wait_begin(ompt_data_t *task_data, ompt_parallel_id_t parallel_id);
void bar_wait_end(ompt_data_t *task_data, ompt_parallel_id_t parallel_id);

#endif

