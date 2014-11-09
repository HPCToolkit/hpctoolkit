#include <pthread.h>
#include <dlfcn.h>

//
// Investigate TBB
//
#include <sched.h>

#include <semaphore.h>
#include <stdio.h>

#include <lib/prof-lean/atomic.h>

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
// No need to grab monitor.h for a single function
//
extern void monitor_real_abort(void);

//
// NOTE 1: the following functions (apparently) need dlvsym instead of dlsym to obtain the 
//         true function:
//           pthread_cond_broadcast
//           pthread_cond_signal
//           pthread_cond_wait
//           pthread_cond_timedwait;
//
//  !! This treatment is determined empirically !!
//  Future systems may need different treatment (or different version for dlvsym)
//
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
// Define strategies for overrides
//

#define pthread_cond_timedwait_REAL   DLV
#define pthread_cond_wait_REAL        DLV
#define pthread_cond_broadcast_REAL   DLV
#define pthread_cond_signal_REAL      DLV

#define pthread_mutex_lock_REAL       ALT
#define pthread_mutex_unlock_REAL     ALT
#define pthread_mutex_timedlock_REAL  DL
#define pthread_mutex_trylock_REAL    ALT

#define pthread_spin_lock_REAL        DL
#define pthread_spin_unlock_REAL      DL

//
// TBB investigation
//

#define sched_yield_REAL              DL
#define sem_wait_REAL                 DL
#define sem_post_REAL                 DL
#define sem_timedwait_REAL            DL

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
REAL_TYPEDEF(int, pthread_mutex_timedlock)(pthread_mutex_t* restrict mutex,
                                           const struct timespec* restrict abs_timeout);
REAL_TYPEDEF(int, pthread_spin_lock)(pthread_spinlock_t* lock);
REAL_TYPEDEF(int, pthread_spin_unlock)(pthread_spinlock_t* lock);


REAL_DCL(pthread_cond_timedwait);
REAL_DCL(pthread_cond_wait);
REAL_DCL(pthread_cond_broadcast);
REAL_DCL(pthread_cond_signal);
REAL_DCL(pthread_mutex_lock);
REAL_DCL(pthread_mutex_unlock);
REAL_DCL(pthread_mutex_timedlock);
REAL_DCL(pthread_spin_lock);
REAL_DCL(pthread_spin_unlock);

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
                                     const struct timespec* restrict abs_timeout)
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

int
OVERRIDE_NM(pthread_spin_lock)(pthread_spinlock_t* lock)
{
  REAL_INIT(pthread_spin_lock);

  TMSG(LOCKWAIT, "pthread_spin_lock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return REAL_FN(pthread_spin_lock)(lock);
  }

  TMSG(LOCKWAIT, "pthread SPIN LOCK override");
  pthread_directed_blame_shift_spin_start((void*) lock);
  int retval = REAL_FN(pthread_spin_lock)((void*) lock);
  pthread_directed_blame_shift_end();

  return retval;
}

int
OVERRIDE_NM(pthread_spin_unlock)(pthread_spinlock_t* lock)
{
  REAL_INIT(pthread_spin_unlock);

  TMSG(LOCKWAIT, "pthread_spin_unlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return REAL_FN(pthread_spin_unlock)(lock);
  }

  TMSG(LOCKWAIT, "pthread SPIN UNLOCK");
  int retval = REAL_FN(pthread_spin_unlock)((void*) lock);
  pthread_directed_blame_accept((void*) lock);
  return retval;
}

//
// (dlsym-based) lookup utility for lazy initialization
//

void*
override_lookup(char* fname)
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
// (dlvsym-based) lookup utility for lazy initialization
//

void*
override_lookupv(char* fname)
{
  dlerror(); // clear dlerror
  void* rv = dlvsym(RTLD_NEXT, fname, "GLIBC_2.3.2");
  char* e = dlerror();
  if (e) {
    hpcrun_abort("dlsym(RTLD_NEXT, %s) failed: %s", fname, e);
  }
  return rv;
}

//
// Further determination of TBB mechanisms
//

// **** global vars for now ****
//

static unsigned int calls_to_sched_yield = 0;
static unsigned int calls_to_sem_wait    = 0;

REAL_TYPEDEF(int, sched_yield)(void);
REAL_DCL(sched_yield);

int
OVERRIDE_NM(sched_yield)(void)
{
  REAL_INIT(sched_yield);

  // TMSG(TBB_EACH, "sched_yield hit");

  int retval = REAL_FN(sched_yield)();

  // __sync_fetch_and_add(&calls_to_sched_yield, 1);
  return retval;
}

REAL_TYPEDEF(int, sem_wait)(sem_t* sem);
REAL_TYPEDEF(int, sem_post)(sem_t* sem);
REAL_TYPEDEF(int, sem_timedwait)(sem_t* sem, const struct timespec* abs_timeout);


REAL_DCL(sem_wait);
REAL_DCL(sem_post);
REAL_DCL(sem_timedwait);

int
OVERRIDE_NM(sem_wait)(sem_t* sem)
{
  REAL_INIT(sem_wait);

  if (! pthread_blame_lockwait_enabled() ) {
    return REAL_FN(sem_wait)(sem);
  }
  TMSG(TBB_EACH, "sem wait hit, sem = %p", sem);
  pthread_directed_blame_shift_blocked_start(sem);
  int retval = REAL_FN(sem_wait)(sem);
  pthread_directed_blame_shift_end();

  // hpcrun_atomicIncr(&calls_to_sem_wait);
  // calls_to_sem_wait++;
  return retval;
}

int
OVERRIDE_NM(sem_post)(sem_t* sem)
{
  REAL_INIT(sem_post);

  TMSG(LOCKWAIT, "sem_post ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return REAL_FN(sem_post)(sem);
  }
  TMSG(LOCKWAIT, "sem POST");
  int retval = REAL_FN(sem_post)(sem);
  pthread_directed_blame_accept(sem);
  // TMSG(TBB_EACH, "sem post hit, sem = %p", sem);

  return retval;
}

int
OVERRIDE_NM(sem_timedwait)(sem_t* sem, const struct timespec* abs_timeout)
{
  REAL_INIT(sem_timedwait);

  TMSG(TBB_EACH, "sem timedwait hit, sem = %p", sem);
  int retval = REAL_FN(sem_timedwait)(sem, abs_timeout);

  return retval;
}

void
tbb_stats(void)
{
  AMSG("TBB stuff: ");
  AMSG("Calls to sched yield: %d", calls_to_sched_yield);
  AMSG("Calls to sem wait", calls_to_sem_wait);
}
