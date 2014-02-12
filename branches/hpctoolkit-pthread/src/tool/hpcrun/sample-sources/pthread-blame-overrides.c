#include <assert.h>
#include <pthread.h>
#include <dlfcn.h>

#include <sample-sources/blame-shift/blame-map.h>
#include <sample-sources/pthread-blame.h>
#include <hpcrun/thread_data.h>

// monitor override macros
#include <monitor-exts/monitor_ext.h>

#include <hpcrun/hpctoolkit.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <messages/messages.h>

/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/


// ***** dl system macros *****
#define DL_REAL_VAR(name) __ ## name

#define DL_REAL_FN(name) __ ## name

#define DL_TYPE(name) __ ## name ## _t

#define DL_EXTERN(rt, name, ...) extern rt DL_REAL_FN(name)(__VA_ARGS__)

#define DL_TYPEDEF(rt, name, ...) typedef rt (*DL_TYPE(name))(__VA_ARGS__)

#define DL_STATIC(name) DL_TYPE(name) DL_REAL_VAR(name)

#define DL_LOOKUPV(name) \
  DL_REAL_VAR(name) = (DL_TYPE(name)) dlvsym(RTLD_NEXT, # name , "GLIBC_2.3.2")

#define DL_LOOKUP(name) \
  DL_REAL_VAR(name) = (DL_TYPE(name)) dlsym(RTLD_NEXT, # name)

#define DL_DEFAULT(name) \
  if (! DL_REAL_VAR(name)) DL_REAL_VAR(name) = (DL_TYPE(name)) 0xDEADBEEF

#define DL_ASSERT(name) \
     if (!DL_REAL_VAR(name)) DL_LOOKUP(name); assert(DL_REAL_VAR(name)) 

// ******************************************************************************
// *draft wrapper interface for pthread routines 
// ****************************************************************************
#ifdef REAL_COND_FNS_AVAIL // used to be COND_AVAIL
extern int DL_REAL_FN(pthread_cond_timedwait)(pthread_cond_t* restrict cond,
                                              pthread_mutex_t* restrict mutex,
                                              const struct timespec* restrict abstime);

extern int DL_REAL_FN(pthread_cond_wait)(pthread_cond_t* restrict cond,
                                         pthread_mutex_t* restrict mutex);

extern int DL_REAL_FN(pthread_cond_broadcast)(pthread_cond_t* cond);

extern int DL_REAL_FN(pthread_cond_signal)(pthread_cond_t* cond);

#else 
typedef int (*DL_TYPE(pthread_cond_timedwait))(pthread_cond_t* restrict cond,
                                               pthread_mutex_t* restrict mutex,
                                               const struct timespec* restrict abstime);
typedef int (*DL_TYPE(pthread_cond_wait))(pthread_cond_t *restrict cond,
                                          pthread_mutex_t *restrict mutex);
typedef int (*DL_TYPE(pthread_cond_broadcast))(pthread_cond_t *cond);
typedef int (*DL_TYPE(pthread_cond_signal))(pthread_cond_t *cond);

static DL_TYPE(pthread_cond_timedwait) DL_REAL_VAR(pthread_cond_timedwait);
static DL_TYPE(pthread_cond_wait) DL_REAL_VAR(pthread_cond_wait);
static DL_TYPE(pthread_cond_broadcast) DL_REAL_VAR(pthread_cond_broadcast);
static DL_TYPE(pthread_cond_signal) DL_REAL_VAR(pthread_cond_signal);

#endif

// mutex function overrides

#define REAL_MUTEX_FNS_AVAIL
#ifdef REAL_MUTEX_FNS_AVAIL
extern int DL_REAL_FN(pthread_mutex_lock)(pthread_mutex_t* mutex);
extern int DL_REAL_FN(pthread_mutex_unlock)(pthread_mutex_t* mutex);
// extern int DL_REAL_FN(pthread_mutex_timedlock)(pthread_mutex_t *restrict mutex,
//                                                const struct timespec *restrict abs_timeout);
extern int DL_REAL_FN(pthread_mutex_trylock)(pthread_mutex_t* mutex);
#else
typedef int (*DL_TYPE(pthread_mutex_lock))(pthread_mutex_t* mutex);
typedef int (*DL_TYPE(pthread_mutex_unlock))(pthread_mutex_t* mutex);

static DL_TYPE(pthread_mutex_lock) DL_REAL_VAR(pthread_mutex_lock);
static DL_TYPE(pthread_mutex_unlock) DL_REAL_VAR(pthread_mutex_unlock);
#endif
typedef int (*DL_TYPE(pthread_mutex_timedlock))(pthread_mutex_t *restrict mutex,
                                                const struct timespec *restrict abs_timeout);
static DL_TYPE(pthread_mutex_timedlock) DL_REAL_VAR(pthread_mutex_timedlock);

 __attribute__ ((constructor)) 
void
pthread_plugin_init() 
{
#ifndef REAL_COND_FNS_AVAIL
  DL_LOOKUPV(pthread_cond_broadcast);
  DL_LOOKUPV(pthread_cond_signal);
  DL_LOOKUPV(pthread_cond_wait);
  DL_LOOKUPV(pthread_cond_timedwait);

#endif
#ifndef REAL_MUTEX_FNS_AVAIL
  DL_LOOKUP(pthread_mutex_lock);
  DL_LOOKUP(pthread_mutex_unlock);
  //  DL_LOOKUP(pthread_mutex_timedlock);
#endif
  DL_LOOKUP(pthread_mutex_timedlock);
}

int 
pthread_cond_timedwait(pthread_cond_t* restrict cond,
                       pthread_mutex_t* restrict mutex,
                       const struct timespec* restrict abstime)
{
  int retval;
  pthread_directed_blame_shift_blocked_start(cond);
  retval = __pthread_cond_timedwait(cond, mutex, abstime);
  pthread_directed_blame_shift_end();
  return retval;
}

int 
pthread_cond_wait(pthread_cond_t* restrict cond,
                  pthread_mutex_t* restrict mutex)
{
  int retval;
  pthread_directed_blame_shift_blocked_start(cond);
  retval = __pthread_cond_wait(cond, mutex);
  pthread_directed_blame_shift_end();
  return retval;
}

int 
pthread_cond_broadcast(pthread_cond_t *cond)
{
  int retval = __pthread_cond_broadcast(cond);
  pthread_directed_blame_accept(cond);
  return retval;
}

int 
pthread_cond_signal(pthread_cond_t* cond)
{
  int retval = __pthread_cond_signal(cond);
  pthread_directed_blame_accept(cond);
  return retval;
}

int 
MONITOR_EXT_WRAP_NAME(pthread_mutex_lock)(pthread_mutex_t* mutex)
{
  TMSG(LOCKWAIT, "mutex lock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return __pthread_mutex_lock(mutex);
  }
  TMSG(LOCKWAIT, "pthread mutex LOCK override");
  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = __pthread_mutex_lock(mutex);
  pthread_directed_blame_shift_end();
  return retval;
}

int 
MONITOR_EXT_WRAP_NAME(pthread_mutex_unlock)(pthread_mutex_t* mutex)
{
  TMSG(LOCKWAIT, "mutex unlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return __pthread_mutex_unlock(mutex);
  }
  TMSG(LOCKWAIT, "pthread mutex UNLOCK");
  int retval = __pthread_mutex_unlock(mutex);
  pthread_directed_blame_accept(mutex);
  return retval;
}

int 
MONITOR_EXT_WRAP_NAME(pthread_mutex_timedlock)(pthread_mutex_t* restrict mutex,
                                               const struct timespec *restrict abs_timeout)
{
  TMSG(LOCKWAIT, "mutex timedlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return __pthread_mutex_timedlock(mutex, abs_timeout);
  }

  TMSG(LOCKWAIT, "pthread mutex TIMEDLOCK");

  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = __pthread_mutex_timedlock(mutex, abs_timeout);
  pthread_directed_blame_shift_end();
  return retval;
}
