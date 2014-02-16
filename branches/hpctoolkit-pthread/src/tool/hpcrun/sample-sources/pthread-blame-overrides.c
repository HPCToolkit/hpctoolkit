#include <pthread.h>
#include <dlfcn.h>

#include <sample-sources/blame-shift/blame-map.h>
#include <sample-sources/pthread-blame.h>
#include <hpcrun/thread_data.h>

// convenience macros to simplify overrides
#include <monitor-exts/overrides.h>

#include <hpcrun/hpctoolkit.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <messages/messages.h>

//
// NOTE 1: the following functions (apparently) need dlvsym to obtain the function
//         via dl lookup:
//           pthread_cond_broadcast
//           pthread_cond_signal
//           pthread_cond_wait
//           pthread_cond_timedwait;
//
// The lookup macro was defined as:
// #define DL_LOOKUPV(name) \
//  DL_FN(name) = (DL_TYPE(name)) dlvsym(RTLD_NEXT, # name , "GLIBC_2.3.2")
//
// An informal check indicates that lookup is NOT necessary for any of the
// systems of interest. *IF THAT CHANGES*, then redeploy above macro as needed
//
// NOTE 2: Rather than use a constructor, use lazy initialization.
//         If constructor needed, uncomment code below
//
// __attribute__ ((constructor)) 
// void
// pthread_plugin_init() 
// {
//   DL_LOOKUPV(pthread_cond_broadcast);
//   DL_LOOKUPV(pthread_cond_signal);
//   DL_LOOKUPV(pthread_cond_wait);
//   DL_LOOKUPV(pthread_cond_timedwait);
//
//   DL_LOOKUP(pthread_mutex_lock);
//   DL_LOOKUP(pthread_mutex_unlock);
//   DL_LOOKUP(pthread_mutex_timedlock);
// }
//

//
// (dlsym-based) lookup utility for lazy initialization
//

static
void*
lookup(char* fname)
{
  dlerror(); // clear dlerror
  void* rv = dlsym(RTLD_NEXT, fname);
  char* e = dlerror();
  if (e) {
    hpcrun_abort("dlsym(RTLD_NEXT, %s) failed: %s", fname, e);
  }
  return rv;
}

//
// Define strategies for overrides
//

#define pthread_cond_timedwait_REAL   ALT
#define pthread_cond_wait_REAL        ALT
#define pthread_cond_broadcast_REAL   ALT
#define pthread_cond_signal_REAL      ALT

#define pthread_mutex_lock_REAL       ALT
#define pthread_mutex_unlock_REAL     ALT
#define pthread_mutex_timedlock_REAL  DL
#define pthread_mutex_trylock_REAL    ALT

// 
// Typedefs and declarations for real routines that are overridden
// 

REAL_TYPEDEF(int, pthread_cond_timedwait)(pthread_cond_t* restrict cond,
                                          pthread_mutex_t* restrict mutex,
                                          const struct timespec* restrict abstime);
REAL_TYPEDEF(int, pthread_cond_wait)(pthread_cond_t* restrict cond,
                                     pthread_mutex_t* restrict mutex);
REAL_TYPEDEF(int, pthread_cond_broadcast)(pthread_cond_t* cond);
REAL_TYPEDEF(int, pthread_cond_signal)(pthread_cond_t* cond);

REAL_TYPEDEF(int, pthread_mutex_lock)(pthread_mutex_t* mutex);
REAL_TYPEDEF(int, pthread_mutex_unlock)(pthread_mutex_t* mutex);
REAL_TYPEDEF(int, pthread_mutex_timedlock)(pthread_mutex_t *restrict mutex,
                                           const struct timespec *restrict abs_timeout);

REAL_DCL(pthread_cond_timedwait);
REAL_DCL(pthread_cond_wait);
REAL_DCL(pthread_cond_broadcast);
REAL_DCL(pthread_cond_signal);
REAL_DCL(pthread_mutex_lock);
REAL_DCL(pthread_mutex_unlock);
REAL_DCL(pthread_mutex_timedlock);

int 
OVERRIDE_NM(pthread_cond_timedwait)(pthread_cond_t* restrict cond,
                                    pthread_mutex_t* restrict mutex,
                                    const struct timespec* restrict abstime)
{
  REAL_INIT(pthread_cond_timedwait);

  pthread_directed_blame_shift_blocked_start(cond);
  int retval = REAL_FN(pthread_cond_timedwait)(cond, mutex, abstime);
  pthread_directed_blame_shift_end();

  return retval;
}

int 
OVERRIDE_NM(pthread_cond_wait)(pthread_cond_t* restrict cond,
                               pthread_mutex_t* restrict mutex)
{
  REAL_INIT(pthread_cond_wait);

  pthread_directed_blame_shift_blocked_start(cond);
  int retval = REAL_FN(pthread_cond_wait)(cond, mutex);
  pthread_directed_blame_shift_end();

  return retval;
}

int 
OVERRIDE_NM(pthread_cond_broadcast)(pthread_cond_t *cond)
{
  REAL_INIT(pthread_cond_broadcast);

  int retval = REAL_FN(pthread_cond_broadcast)(cond);
  pthread_directed_blame_accept(cond);
  return retval;
}

int 
OVERRIDE_NM(pthread_cond_signal)(pthread_cond_t* cond)
{
  REAL_INIT(pthread_cond_signal);

  int retval = REAL_FN(pthread_cond_signal)(cond);
  pthread_directed_blame_accept(cond);
  return retval;
}

int 
OVERRIDE_NM(pthread_mutex_lock)(pthread_mutex_t* mutex)
{
  REAL_INIT(pthread_mutex_lock);

  TMSG(LOCKWAIT, "mutex lock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return REAL_FN(pthread_mutex_lock)(mutex);
  }

  TMSG(LOCKWAIT, "pthread mutex LOCK override");
  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = REAL_FN(pthread_mutex_lock)(mutex);
  pthread_directed_blame_shift_end();

  return retval;
}

int 
OVERRIDE_NM(pthread_mutex_unlock)(pthread_mutex_t* mutex)
{
  REAL_INIT(pthread_mutex_unlock);

  TMSG(LOCKWAIT, "mutex unlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return REAL_FN(pthread_mutex_unlock)(mutex);
  }
  TMSG(LOCKWAIT, "pthread mutex UNLOCK");
  int retval = REAL_FN(pthread_mutex_unlock)(mutex);
  pthread_directed_blame_accept(mutex);
  return retval;
}

int 
OVERRIDE_NM(pthread_mutex_timedlock)(pthread_mutex_t* restrict mutex,
                                     const struct timespec *restrict abs_timeout)
{
  REAL_INIT(pthread_mutex_timedlock);
  
  TMSG(LOCKWAIT, "mutex timedlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return REAL_FN(pthread_mutex_timedlock)(mutex, abs_timeout);
  }

  TMSG(LOCKWAIT, "pthread mutex TIMEDLOCK");

  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = REAL_FN(pthread_mutex_timedlock)(mutex, abs_timeout);
  pthread_directed_blame_shift_end();
  return retval;
}
