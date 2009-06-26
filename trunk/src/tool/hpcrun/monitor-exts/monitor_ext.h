/*
 * Macros for extending monitor's overrides.
 *
 * $Id$
 */

#ifndef _MONITOR_EXT_H_
#define _MONITOR_EXT_H_

#include <pthread.h>

#include "pmsg.h"


#ifdef HPCRUN_STATIC_LINK

#define MONITOR_WRAP_NAME(name)   __wrap_ ## name
#define MONITOR_GET_NAME(var, name)  var = & name
#define MONITOR_GET_NAME_WRAP(var, name)  var = & __real_ ## name

#else  /* MONITOR DYNAMIC */

#include <dlfcn.h>

#ifndef RTLD_NEXT
#define RTLD_NEXT  ((void *) -1L)
#endif

#define MONITOR_GET_DLSYM(var, name)  do {		\
    if (var == NULL) {					\
	const char *err_str;				\
	dlerror();					\
	var = dlsym(RTLD_NEXT, #name );			\
	err_str = dlerror();				\
	if (var == NULL) {				\
	    csprof_abort("dlsym(%s) failed: %s", #name , err_str); \
	}						\
	TMSG(MONITOR_EXTS, "%s() = %p", #name , var);	\
    }							\
} while (0)

#define MONITOR_WRAP_NAME(name)  name
#define MONITOR_GET_NAME(var, name)  MONITOR_GET_DLSYM(var, name)
#define MONITOR_GET_NAME_WRAP(var, name)  MONITOR_GET_DLSYM(var, name)

#endif  /* MONITOR_STATIC */


//
// The extra monitor callback functions beyond monitor.h.
//

void monitor_thread_pre_mutex_lock(pthread_mutex_t *lock);
void monitor_thread_post_mutex_lock(pthread_mutex_t *lock, int result);
void monitor_thread_post_mutex_trylock(pthread_mutex_t *lock, int result);
void monitor_thread_post_mutex_unlock(pthread_mutex_t *lock);

void monitor_thread_spin_init(pthread_spinlock_t *lock, int result);
pthread_spinlock_t *monitor_thread_pre_spin_destroy(pthread_spinlock_t *lock);

pthread_spinlock_t *monitor_thread_pre_spin_lock(pthread_spinlock_t *lock);
void monitor_thread_post_spin_lock(pthread_spinlock_t *lock, int result);

pthread_spinlock_t *monitor_thread_pre_spin_trylock(pthread_spinlock_t *lock);
void monitor_thread_post_spin_trylock(pthread_spinlock_t *lock, int result);

pthread_spinlock_t *monitor_thread_pre_spin_unlock(pthread_spinlock_t *lock);
void monitor_thread_post_spin_unlock(pthread_spinlock_t *lock, int result);

void monitor_thread_pre_cond_wait(void);
void monitor_thread_post_cond_wait(int result);


#endif  /* ! _MONITOR_EXT_H_ */
