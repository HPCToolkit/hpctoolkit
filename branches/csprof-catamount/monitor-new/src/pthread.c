/*
 * Monitor pthread functions.
 *
 * $Id$
 */

#include "config.h"
#ifdef MONITOR_DYNAMIC
#include <dlfcn.h>
#endif
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "monitor.h"
#include "queue.h"

/*
 *----------------------------------------------------------------------
 *
 *  Keep a list of running threads so we can invoke their fini thread
 *  functions in the case that one thread calls exit() before the
 *  others call pthread_exit().
 *
 *----------------------------------------------------------------------
 */
#define MONITOR_EXIT_CLEANUP_SIGNAL  SIGUSR2
#define MONITOR_POLL_USLEEP_TIME  150000
#define MONITOR_TN_MAGIC  0x6d746e00

/*
 *  The global thread mutex protects the monitor's list of threads and
 *  related state variables.
 *
 *  Note: After monitor begins exit cleanup (some thread calls exit or
 *  exec), then we don't insert or delete any new thread nodes into
 *  the list.  Instead, we set the tn_* variables to indicate the
 *  thread status and when it is finished.
 */

/*
 *----------------------------------------------------------------------
 *  MONITOR THREAD DEFINITIONS
 *----------------------------------------------------------------------
 */

#ifdef MONITOR_PTHREAD_EQUAL_IS_FCN
#define PTHREAD_EQUAL(t1, t2)  ((*real_pthread_equal)((t1), (t2)))
#else
#define PTHREAD_EQUAL(t1, t2)  pthread_equal((t1), (t2))
#endif
#ifdef MONITOR_PTHREAD_CLEANUP_PUSH_IS_FCN
#define PTHREAD_CLEANUP_PUSH(fcn, arg)  ((*real_pthread_cleanup_push)((fcn), (arg)))
#define PTHREAD_CLEANUP_POP(arg)        ((*real_pthread_cleanup_pop)(arg))
#else
#define PTHREAD_CLEANUP_PUSH(fcn, arg)  pthread_cleanup_push((fcn), (arg))
#define PTHREAD_CLEANUP_POP(arg)        pthread_cleanup_pop(arg)
#endif

/*
 *----------------------------------------------------------------------
 *  MONITOR THREAD DEFINITIONS
 *----------------------------------------------------------------------
 */

#define PTHREAD_CREATE_PARAM_LIST		\
    pthread_t *thread,				\
    const pthread_attr_t *attr,			\
    pthread_start_fcn_t *start_routine,		\
    void *arg

typedef void *pthread_start_fcn_t(void *);
typedef int   pthread_create_fcn_t( PTHREAD_CREATE_PARAM_LIST );
typedef int   pthread_equal_fcn_t(pthread_t, pthread_t);
typedef int   pthread_key_create_fcn_t(pthread_key_t *, void (*)(void *));
typedef int   pthread_kill_fcn_t(pthread_t, int);
typedef pthread_t pthread_self_fcn_t(void);
typedef int   pthread_mutex_lock_fcn_t(pthread_mutex_t *);
typedef void *pthread_getspecific_fcn_t(pthread_key_t);
typedef int   pthread_setspecific_fcn_t(pthread_key_t, const void *);
typedef void  pthread_cleanup_push_fcn_t(void (*)(void *), void *);
typedef void  pthread_cleanup_pop_fcn_t(int);

#ifdef MONITOR_STATIC
extern pthread_create_fcn_t  __real_pthread_create;
#endif

static pthread_create_fcn_t  *real_pthread_create;
static pthread_equal_fcn_t   *real_pthread_equal;
static pthread_key_create_fcn_t   *real_pthread_key_create;
static pthread_kill_fcn_t  *real_pthread_kill;
static pthread_self_fcn_t  *real_pthread_self;
static pthread_mutex_lock_fcn_t  *real_pthread_mutex_lock;
static pthread_mutex_lock_fcn_t  *real_pthread_mutex_unlock;
static pthread_getspecific_fcn_t  *real_pthread_getspecific;
static pthread_setspecific_fcn_t  *real_pthread_setspecific;
static pthread_cleanup_push_fcn_t  *real_pthread_cleanup_push;
static pthread_cleanup_pop_fcn_t   *real_pthread_cleanup_pop;

/*
 *----------------------------------------------------------------------
 *  MONITOR THREAD DEFINITIONS
 *----------------------------------------------------------------------
 */

static pthread_mutex_t monitor_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MONITOR_THREAD_LOCK    (*real_pthread_mutex_lock)(&monitor_thread_mutex)
#define MONITOR_THREAD_UNLOCK  (*real_pthread_mutex_unlock)(&monitor_thread_mutex)

struct monitor_thread_node {
    LIST_ENTRY(monitor_thread_node) tn_links;
    int    tn_magic;
    int    tn_thread_num;
    void  *tn_user_data;
    pthread_t  tn_self;
    pthread_start_fcn_t  *tn_start_routine;
    void  *tn_arg;
    char   tn_appl_started;
    char   tn_fini_started;
    char   tn_fini_done;
};

static LIST_HEAD(, monitor_thread_node) monitor_thread_list;

static pthread_key_t monitor_pthread_key;

volatile static int  monitor_thread_num = 0;
volatile static char monitor_has_reached_main = 0;
volatile static char monitor_has_used_threads = 0;
volatile static char monitor_thread_support_done = 0;
volatile static char monitor_in_exit_cleanup = 0;

extern void monitor_unwind_thread_fence1;
extern void monitor_unwind_thread_fence2;

/*
 *----------------------------------------------------------------------
 *  MONITOR THREAD FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Run in the main thread on the first call to pthread_create(),
 *  before any new threads are created.
 */
static void
monitor_thread_init(void)
{
    int ret;

    /*
     * Give main.c functions a chance to initialize in the case that
     * some library calls pthread_create() before main().
     */
    monitor_early_init();
    MONITOR_DEBUG1("\n");

    LIST_INIT(&monitor_thread_list);
    MONITOR_GET_REAL_NAME_WRAP(real_pthread_create, pthread_create);
#ifdef MONITOR_PTHREAD_EQUAL_IS_FCN
    MONITOR_GET_REAL_NAME(real_pthread_equal, pthread_equal);
#endif
    MONITOR_GET_REAL_NAME(real_pthread_key_create, pthread_key_create);
    MONITOR_GET_REAL_NAME(real_pthread_kill, pthread_kill);
    MONITOR_GET_REAL_NAME(real_pthread_self, pthread_self);
    MONITOR_GET_REAL_NAME(real_pthread_mutex_lock,   pthread_mutex_lock);
    MONITOR_GET_REAL_NAME(real_pthread_mutex_unlock, pthread_mutex_unlock);
    MONITOR_GET_REAL_NAME(real_pthread_getspecific,  pthread_getspecific);
    MONITOR_GET_REAL_NAME(real_pthread_setspecific,  pthread_setspecific);
#ifdef MONITOR_PTHREAD_CLEANUP_PUSH_IS_FCN
    MONITOR_GET_REAL_NAME(real_pthread_cleanup_push, pthread_cleanup_push);
    MONITOR_GET_REAL_NAME(real_pthread_cleanup_pop,  pthread_cleanup_pop);
#endif
    ret = (*real_pthread_key_create)(&monitor_pthread_key, NULL);
    if (ret != 0) {
	MONITOR_ERROR("pthread_key_create failed (%d)\n", ret);
    }
    monitor_has_used_threads = 1;
}

static struct monitor_thread_node *
monitor_make_thread_node(void)
{
    struct monitor_thread_node *tn;

    tn = malloc(sizeof(struct monitor_thread_node));
    if (tn == NULL) {
	MONITOR_ERROR1("unable to malloc thread node\n");
    }
    memset(tn, 0, sizeof(struct monitor_thread_node));
    tn->tn_magic = MONITOR_TN_MAGIC;

    return (tn);
}

/*
 *  Runs in the new thread.
 * 
 *  Returns: 0 on success, or 1 if at exit cleanup and thus we don't
 *  allow any new threads.
 */ 
static int
monitor_link_thread_node(struct monitor_thread_node *tn)
{
    int ret;

    MONITOR_THREAD_LOCK;
    if (monitor_in_exit_cleanup) {
	MONITOR_THREAD_UNLOCK;
	return (1);
    }

    tn->tn_thread_num = ++monitor_thread_num;
    tn->tn_self= (*real_pthread_self)();
    LIST_INSERT_HEAD(&monitor_thread_list, tn, tn_links);
    ret = (*real_pthread_setspecific)(monitor_pthread_key, tn);
    if (ret != 0) {
	MONITOR_ERROR("pthread_setspecific failed (%d)\n", ret);
    }

    MONITOR_THREAD_UNLOCK;
    return (0);
}

extern void *unlink_thread_free;

static void
monitor_unlink_thread_node(struct monitor_thread_node *tn)
{
    MONITOR_THREAD_LOCK;

    /*
     * Don't delete the thread node if in exit cleanup, just mark the
     * node as finished.
     */
    if (monitor_in_exit_cleanup)
	tn->tn_fini_done = 1;
    else {
	LIST_REMOVE(tn, tn_links);
	memset(tn, 0, sizeof(struct monitor_thread_node));
	MONITOR_DEBUG1("monitor_unlink_thread: about to call free\n");
#if 0
    asm(".globl unlink_thread_free");
    asm("unlink_thread_free:");
#endif
	free(tn);
    }

    MONITOR_THREAD_UNLOCK;
}

static void
monitor_thread_signal_handler(int signum)
{
    struct monitor_thread_node *tn;

    tn = (*real_pthread_getspecific)(monitor_pthread_key);
    if (tn == NULL || tn->tn_magic != MONITOR_TN_MAGIC) {
	MONITOR_WARN1("missing thread specific data -- "
		      "unable to call monitor_fini_thread\n");
	return;
    }
    if (tn->tn_appl_started && !tn->tn_fini_started) {
	MONITOR_DEBUG("calling monitor_fini_thread(data = %p, num = %d) ...\n",
		      tn->tn_user_data, tn->tn_thread_num);
	tn->tn_fini_started = 1;
	monitor_fini_thread(tn->tn_user_data);
	tn->tn_fini_done = 1;
    }
}

/*
 *  Call monitor_thread_support.
 */
static inline void
monitor_call_thread_support(void)
{
    if (monitor_thread_support_done) {
	MONITOR_WARN1("attempted to call thread support twice\n");
	return;
    }
    MONITOR_DEBUG1("calling monitor_init_thread_support() ...\n");
    monitor_init_thread_support();
    monitor_thread_support_done = 1;
}

/*
 *  Called from main.c at begin process time to release threads
 *  waiting on deferred thread support.
 */
void
monitor_thread_release(void)
{
    if (monitor_has_used_threads)
	monitor_call_thread_support();

    monitor_has_reached_main = 1;
}

/*
 *  Called from main.c at end process time for possible thread cleanup.
 *  Note: main doesn't know if application is threaded or not.
 */
void
monitor_thread_shootdown(void)
{
    struct monitor_thread_node *tn, *my_tn;
    struct sigaction my_action;
    sigset_t empty_set;
    pthread_t self;
    int num_unfinished;

    if (! monitor_has_used_threads)
	return;

    MONITOR_THREAD_LOCK;
    monitor_in_exit_cleanup = 1;
    MONITOR_THREAD_UNLOCK;

    MONITOR_DEBUG1("\n");

    /*
     * Install the signal handler for MONITOR_EXIT_CLEANUP_SIGNAL.
     * Note: the signal handler is process-wide.
     */
    sigemptyset(&empty_set);
    my_action.sa_handler = monitor_thread_signal_handler;
    my_action.sa_mask = empty_set;
    my_action.sa_flags = 0;
    if (sigaction(MONITOR_EXIT_CLEANUP_SIGNAL, &my_action, NULL) != 0) {
	MONITOR_ERROR1("sigaction failed\n");
    }

    /*
     * Walk through the list of unfinished threads, send a signal to
     * force them into their fini_thread functions, and wait until
     * they all finish.  But don't signal ourself.
     *
     * Note: may want to add a timeout here.
     */
    self = (*real_pthread_self)();
    my_tn = NULL;
    for (;;) {
	num_unfinished = 0;
	for (tn = LIST_FIRST(&monitor_thread_list);
	     tn != NULL;
	     tn = LIST_NEXT(tn, tn_links)) {

	     if (PTHREAD_EQUAL(self, tn->tn_self)) {
		my_tn = tn;
		continue;
	    }
	    if (tn->tn_appl_started && !tn->tn_fini_started)
		(*real_pthread_kill)(tn->tn_self, MONITOR_EXIT_CLEANUP_SIGNAL);

	    if (! tn->tn_fini_done)
		num_unfinished++;
	}
	MONITOR_DEBUG("waiting on %d threads to finish\n", num_unfinished);
	if (num_unfinished == 0)
	    break;

	usleep(MONITOR_POLL_USLEEP_TIME);
    }

    /*
     * See if we need to run fini_thread from this thread.
     */
    if (my_tn != NULL && !my_tn->tn_fini_started) {
	MONITOR_DEBUG("calling monitor_fini_thread(data = %p, num = %d) ...\n",
		      tn->tn_user_data, tn->tn_thread_num);
	tn->tn_fini_started = 1;
	monitor_fini_thread(tn->tn_user_data);
	tn->tn_fini_done = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *  MONITOR PTHREAD_CREATE OVERRIDE and HELPER FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  We get here when the application thread has finished, either by
 *  returning, pthread_exit() or being canceled.
 */
static void
monitor_pthread_cleanup_routine(void *arg)
{
    struct monitor_thread_node *tn = arg;

    MONITOR_DEBUG1("(pthread cleanup)\n");
    if (tn == NULL || tn->tn_magic != MONITOR_TN_MAGIC) {
	MONITOR_WARN1("missing thread-specific data\n");
	MONITOR_DEBUG1("calling monitor_fini_thread(NULL)\n");
	monitor_fini_thread(NULL);
	return;
    }

    if (! tn->tn_fini_started) {
	MONITOR_DEBUG("calling monitor_fini_thread(data = %p), num = %d\n",
		      tn->tn_user_data, tn->tn_thread_num);
	monitor_fini_thread(tn->tn_user_data);
    }
    monitor_unlink_thread_node(tn);
}

/*
 *  Called from real_pthread_create(), it's where the newly-created
 *  thread begins.
 */
static void *
monitor_pthread_start_routine(void *arg)
{
    struct monitor_thread_node *tn = arg;
    void *ret;

    /*
     * Wait for monitor_init_thread_support() to finish in the main
     * thread before this thread runs.
     *
     * Note: if this thread is created before libc_start_main (OpenMP
     * does this), then this will wait for both init_process and
     * thread_support to finish from libc_start_main.  And that has
     * the potential to deadlock if the application waits for this
     * thread to accomplish something before it finishes its library
     * init.  (An evil thing for it to do, but it's possible.)
     */
    while (! monitor_thread_support_done)
	usleep(MONITOR_POLL_USLEEP_TIME);

    MONITOR_DEBUG1("(start of new thread)\n");

    /*
     * Don't create any new threads after someone has called exit().
     */
    if (monitor_link_thread_node(tn) != 0) {
	MONITOR_WARN1("trying to create new thread during exit cleanup -- "
		      "thread not started\n");
	return (NULL);
    }

    asm("monitor_unwind_thread_fence1:");
    MONITOR_DEBUG("calling monitor_init_thread(num = %u) ...\n",
		  tn->tn_thread_num);
    tn->tn_user_data = monitor_init_thread(tn->tn_thread_num);

    PTHREAD_CLEANUP_PUSH(monitor_pthread_cleanup_routine, tn);
    tn->tn_appl_started = 1;
    ret = (tn->tn_start_routine)(tn->tn_arg);
    PTHREAD_CLEANUP_POP(1);
    asm("monitor_unwind_thread_fence2:");

    return (ret);
}

/*
 *  Returns: 1 if address is at the bottom of the thread's call stack,
 *  else 0.
 */
int
monitor_unwind_thread_bottom_frame(void *addr)
{
    return (&monitor_unwind_thread_fence1 <= addr &&
	    addr <= &monitor_unwind_thread_fence2);
}

/*
 *  Override pthread_create().
 */
int
MONITOR_WRAP_NAME(pthread_create) ( PTHREAD_CREATE_PARAM_LIST )
{
    struct monitor_thread_node *tn;
    int first, ret;

    /*
     * There is no race condition to get here first because until now,
     * there is only one thread.
     */
    first = !monitor_has_used_threads;
    if (first) {
	monitor_thread_init();
	monitor_has_used_threads = 1;
    }

    MONITOR_DEBUG1("\n");

    /*
     * Always launch the thread.  If we're in pthread_create() too
     * early, then the new thread will spin-wait until init_process
     * and thread_support are done.
     */
    tn = monitor_make_thread_node();
    tn->tn_start_routine = start_routine;
    tn->tn_arg = arg;

    ret = (*real_pthread_create)
	(thread, attr, monitor_pthread_start_routine, (void *)tn);

    if (first) {
	/*
	 * Normally, we run thread_support here, on the first call to
	 * pthread_create().  But if we're here early, before
	 * libc_start_main, then defer thread_support until after
	 * init_process in libc_start_main.
	 */
	if (monitor_has_reached_main) {
	    monitor_call_thread_support();
	} else {
	    MONITOR_DEBUG1("deferring thread support\n");
	}
    }

    return (ret);
}

void monitor_debug_pthread(void){
#if 0
  MONITOR_DEBUG("addr of monitor_unlink_thread_node = %p\n",&monitor_unlink_thread_node);
  MONITOR_DEBUG("addr of unlink_thread free call = %p\n",&unlink_thread_free);
#endif
}
