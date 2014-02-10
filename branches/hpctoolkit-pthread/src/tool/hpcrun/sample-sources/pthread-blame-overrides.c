#include <assert.h>
#include <pthread.h>
#include <dlfcn.h>

#include <sample-sources/blame-shift/blame-map.h>
#include <sample-sources/pthread-blame.h>
#include <hpcrun/thread_data.h>

#include <hpcrun/hpctoolkit.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <messages/messages.h>

/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

typedef struct {
  uint64_t target;
  bool idle;
} blame_t;

static __thread blame_t pthread_blame = {0, false};

static void
directed_blame_shift_start(void* obj)
{
  TMSG(LOCKWAIT, "Start directed blaming using blame structure %x, for obj %d",
       &pthread_blame, (uintptr_t) obj);
  pthread_blame = (blame_t) {.target = (uint64_t)(uintptr_t)obj,
                             .idle   = true};
}


static void
directed_blame_shift_end(void)
{
  pthread_blame = (blame_t) {.target = 0, .idle = false};
  TMSG(LOCKWAIT, "End directed blaming for blame structure %x",
       &pthread_blame);
}

static void
directed_blame_accept(void* obj)
{
  uint64_t blame = blame_map_get_blame((uint64_t) (uintptr_t) obj);
  TMSG(LOCKWAIT, "Blame obj %d accepting %d units of blame", obj, blame);
  if (blame && hpctoolkit_sampling_is_active()) {
    ucontext_t uc;
    getcontext(&uc);
    hpcrun_safe_enter();
    hpcrun_sample_callpath(&uc, hpcrun_get_pthread_directed_blame_metric_id(),
                           blame, 0, 1);
    hpcrun_safe_exit();
  }
}

// ***********************
// public interface to blame structure
// ***********************

uint64_t
pthread_blame_get_blame_target(void)
{
  return pthread_blame.target;
}

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
#ifdef REAL_FNS_AVAIL // used to be COND_AVAIL
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

#ifdef REAL_FNS_AVAIL
extern int DL_REAL_FN(pthread_mutex_lock)(pthread_mutex_t* mutex);
extern int DL_REAL_FN(pthread_mutex_unlock)(pthread_mutex_t* mutex);
extern int DL_REAL_FN(pthread_mutex_timedlock)(pthread_mutex_t *restrict mutex,
                                               const struct timespec *restrict abs_timeout);
#else
typedef int (*DL_TYPE(pthread_mutex_lock))(pthread_mutex_t* mutex);
typedef int (*DL_TYPE(pthread_mutex_unlock))(pthread_mutex_t* mutex);
typedef int (*DL_TYPE(pthread_mutex_timedlock))(pthread_mutex_t *restrict mutex,
                                                const struct timespec *restrict abs_timeout);

static DL_TYPE(pthread_mutex_lock) DL_REAL_VAR(pthread_mutex_lock);
static DL_TYPE(pthread_mutex_unlock) DL_REAL_VAR(pthread_mutex_unlock);
static DL_TYPE(pthread_mutex_timedlock) DL_REAL_VAR(pthread_mutex_timedlock);
#endif

 __attribute__ ((constructor)) 
void
pthread_plugin_init() 
{
#ifdef COND_AVAIL
  DL_LOOKUPV(pthread_cond_broadcast);
  DL_LOOKUPV(pthread_cond_signal);
  DL_LOOKUPV(pthread_cond_wait);
  DL_LOOKUPV(pthread_cond_timedwait);

#endif
  DL_LOOKUP(pthread_mutex_lock);
  DL_LOOKUP(pthread_mutex_unlock);
  DL_LOOKUP(pthread_mutex_timedlock);
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
  if (! pthread_blame_lockwait_enabled() ) {
    return __pthread_mutex_lock(mutex);
  }
  TMSG(LOCKWAIT, "pthread mutex LOCK");
  directed_blame_shift_start(mutex);
  //
  // trylock here
  // replace sleepwait with busywait
  //
  int retval = __pthread_mutex_trylock(mutex);
  while(! (retval = __pthread_mutex_trylock(mutex))){
    TMSG(LOCKWAIT, "Waiting for mutex lock");
  }
  //
  // Original replacement call
  // int retval = __pthread_mutex_lock(mutex);
  //
  return retval;
  directed_blame_shift_end();
  return retval;
}

int 
pthread_mutex_unlock(pthread_mutex_t* mutex)
{
  if (! pthread_blame_lockwait_enabled() ) {
    return __pthread_mutex_lock(mutex);
  }
  TMSG(LOCKWAIT, "pthread mutex UNLOCK");
  int retval = __pthread_mutex_unlock(mutex);
  directed_blame_accept(mutex);
  return retval;
}

int 
pthread_mutex_timedlock(pthread_mutex_t* restrict mutex,
                        const struct timespec *restrict abs_timeout)
{
  if (! pthread_blame_lockwait_enabled() ) {
    return __pthread_mutex_lock(mutex);
  }

  TMSG(LOCKWAIT, "pthread mutex TIMEDLOCK");

  directed_blame_shift_start(mutex);
  // how to do this ??
  int retval = __pthread_mutex_timedlock(mutex, abs_timeout);
  directed_blame_shift_end();
  return retval;
}
