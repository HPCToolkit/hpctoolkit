#include <pthread.h>
#include <dlfcn.h>

#include <sample-sources/blame-shift/blame-map.h>
#include <sample-sources/pthread-blame.h>
#include <hpcrun/thread_data.h>

#include <hpcrun/hpctoolkit.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>

/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

static void
directed_blame_shift_start(void* obj)
{
  thread_data_t* td = hpcrun_get_thread_data();
  td->blame_target = (uint64_t) (uintptr_t) obj;
  // idle_metric_blame_shift_idle();
}


static void
directed_blame_shift_end(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  td->blame_target = 0;
  // idle_metric_blame_shift_work();
}

//
// BLAME-SHIFT FIXME: obtain the blame shift object
//

static void
directed_blame_accept(void* obj)
{
  uint64_t blame = blame_map_get_blame((uint64_t) (uintptr_t) obj);
  if (blame != 0 && hpctoolkit_sampling_is_active()) {
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_safe_enter();
    hpcrun_sample_callpath(&uc, hpcrun_get_pthread_directed_blame_metric_id(),
                           blame, 0, 1);
    hpcrun_safe_exit();
  }
}

/******************************************************************************
 * draft wrapper interface for pthread routines 
 *****************************************************************************/
#ifdef COND_AVAIL
extern int __pthread_cond_timedwait(pthread_cond_t *restrict cond,
                                    pthread_mutex_t *restrict mutex,
                                    const struct timespec *restrict abstime);

extern int __pthread_cond_wait(pthread_cond_t *restrict cond,
                               pthread_mutex_t *restrict mutex);

extern int __pthread_cond_broadcast(pthread_cond_t *cond);

extern int __pthread_cond_signal(pthread_cond_t *cond);
#else 

typedef int (*__pthread_cond_timedwait_t)(pthread_cond_t *restrict cond,
                                    pthread_mutex_t *restrict mutex,
                                    const struct timespec *restrict abstime);

typedef int (*__pthread_cond_wait_t)(pthread_cond_t *restrict cond,
                               pthread_mutex_t *restrict mutex);

typedef int (*__pthread_cond_broadcast_t)(pthread_cond_t *cond);

typedef int (*__pthread_cond_signal_t)(pthread_cond_t *cond);

__pthread_cond_timedwait_t __pthread_cond_timedwait;
__pthread_cond_wait_t __pthread_cond_wait;
__pthread_cond_broadcast_t __pthread_cond_broadcast;
__pthread_cond_signal_t __pthread_cond_signal;
#endif

extern int __pthread_mutex_lock(pthread_mutex_t *mutex);

extern int __pthread_mutex_unlock(pthread_mutex_t *mutex);

extern int __pthread_mutex_timedlock(pthread_mutex_t *restrict mutex,
                                     const struct timespec *restrict abs_timeout);



#define DL_LOOKUP(name) \
  __ ## name = (__ ## name ## _t ) dlvsym(RTLD_NEXT, # name , "GLIBC_2.3.2") 
void __attribute__ ((constructor))
pthread_plugin_init() 
{
#ifdef REGISTER_BLAME_SOURCE // FIXME BLAME ?????
  idle_metric_register_blame_source();
#endif // REGISTER_BLAME_SOURCE
#ifndef COND_AVAIL
   DL_LOOKUP(pthread_cond_broadcast);
   DL_LOOKUP(pthread_cond_signal);
   DL_LOOKUP(pthread_cond_wait);
   DL_LOOKUP(pthread_cond_timedwait);
#endif
}

int 
pthread_cond_timedwait(pthread_cond_t* restrict cond,
                       pthread_mutex_t* restrict mutex,
                       const struct timespec* restrict abstime)
{
  int retval;
  directed_blame_shift_start(cond);
  retval = __pthread_cond_timedwait(cond, mutex, abstime);
  directed_blame_shift_end();
  return retval;
}


int 
pthread_cond_wait(pthread_cond_t* restrict cond,
                  pthread_mutex_t* restrict mutex)
{
  int retval;
  directed_blame_shift_start(cond);
  retval = __pthread_cond_wait(cond, mutex);
  directed_blame_shift_end();
  return retval;
}


int 
pthread_cond_broadcast(pthread_cond_t *cond)
{
  int retval = __pthread_cond_broadcast(cond);
  directed_blame_accept(cond);
  return retval;
}


int 
pthread_cond_signal(pthread_cond_t* cond)
{
  int retval = __pthread_cond_signal(cond);
  directed_blame_accept(cond);
  return retval;
}


int 
pthread_mutex_lock(pthread_mutex_t* mutex)
{
  int retval;
  directed_blame_shift_start(mutex);
  retval = __pthread_mutex_lock(mutex);
  directed_blame_shift_end();
  return retval;
}


int 
pthread_mutex_unlock(pthread_mutex_t* mutex)
{
  int retval = __pthread_mutex_unlock(mutex);
  directed_blame_accept(mutex);
  return retval;
}


int 
pthread_mutex_timedlock(pthread_mutex_t* restrict mutex,
                        const struct timespec *restrict abs_timeout)
{
  int retval;
  directed_blame_shift_start(mutex);
  retval = __pthread_mutex_timedlock(mutex, abs_timeout);
  directed_blame_shift_end();
  return retval;
}
