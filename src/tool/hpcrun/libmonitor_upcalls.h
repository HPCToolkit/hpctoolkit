//
// The extra monitor callback functions beyond monitor.h.
//
// $Id$
//

#ifndef _LIBMONITOR_UPCALLS_H_
#define _LIBMONITOR_UPCALLS_H_

void monitor_thread_pre_lock(void);
void monitor_thread_post_lock(int result);
void monitor_thread_post_trylock(int result);
void monitor_thread_unlock(void);
void monitor_thread_pre_cond_wait(void);
void monitor_thread_post_cond_wait(int result);

#endif  // ! _LIBMONITOR_UPCALLS_H_
