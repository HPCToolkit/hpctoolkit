#ifndef _IDLE_H_
#define _IDLE_H_

typedef int (*idle_blame_participant_fn)(void);

void idle_metric_register_blame_source
(idle_blame_participant_fn participant);

void idle_metric_blame_shift_idle(void);
void idle_metric_blame_shift_work(void);

void idle_metric_thread_start(void);
void idle_metric_thread_end(void);

void idle_metric_enable(); 

#endif

