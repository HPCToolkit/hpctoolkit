/*
 *  Libmonitor pthread functions.
 *
 *  Copyright (c) 2007-2008, Rice University.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  * Neither the name of Rice University (RICE) nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  This software is provided by RICE and contributors "as is" and any
 *  express or implied warranties, including, but not limited to, the
 *  implied warranties of merchantability and fitness for a particular
 *  purpose are disclaimed. In no event shall RICE or contributors be
 *  liable for any direct, indirect, incidental, special, exemplary, or
 *  consequential damages (including, but not limited to, procurement of
 *  substitute goods or services; loss of use, data, or profits; or
 *  business interruption) however caused and on any theory of liability,
 *  whether in contract, strict liability, or tort (including negligence
 *  or otherwise) arising in any way out of the use of this software, even
 *  if advised of the possibility of such damage.
 *
 *  $Id: pthread.c 81 2008-04-22 04:38:34Z krentel $
 *
 *  Override functions:
 *
 *    pthread_create
 *    pthread_sigmask
 *
 *  Support functions:
 *
 *    monitor_is_threaded
 *    monitor_get_user_data
 *    monitor_get_thread_num
 *    monitor_stack_bottom
 *    monitor_in_start_func_wide
 *    monitor_in_start_func_narrow
 *    monitor_real_pthread_sigmask
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
#include <unistd.h>

#include "common.h"
#include "monitor.h"
#include "pthread_h.h"
#include "queue.h"

/*
 *----------------------------------------------------------------------
 *  GLOBAL VARIABLES and EXTERNAL DECLARATIONS
 *----------------------------------------------------------------------
 */

#define MONITOR_EXIT_CLEANUP_SIGNAL  SIGUSR2

/*
 *  On some systems, pthread_equal() and pthread_cleanup_push/pop()
 *  are macros and sometimes they're library functions.
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
 *  External functions.
 */
#define PTHREAD_CREATE_PARAM_LIST		\
    pthread_t *thread,				\
    const pthread_attr_t *attr,			\
    pthread_start_fcn_t *start_routine,		\
    void *arg

typedef int   pthread_create_fcn_t(PTHREAD_CREATE_PARAM_LIST);
typedef int   pthread_equal_fcn_t(pthread_t, pthread_t);
typedef int   pthread_key_create_fcn_t(pthread_key_t *, void (*)(void *));
typedef int   pthread_key_delete_fcn_t(pthread_key_t);
typedef int   pthread_kill_fcn_t(pthread_t, int);
typedef pthread_t pthread_self_fcn_t(void);
typedef int   pthread_mutex_lock_fcn_t(pthread_mutex_t *);
typedef void *pthread_getspecific_fcn_t(pthread_key_t);
typedef int   pthread_setspecific_fcn_t(pthread_key_t, const void *);
typedef void  pthread_cleanup_push_fcn_t(void (*)(void *), void *);
typedef void  pthread_cleanup_pop_fcn_t(int);
typedef int   sigaction_fcn_t(int, const struct sigaction *,
			      struct sigaction *);
typedef int   sigprocmask_fcn_t(int, const sigset_t *, sigset_t *);

#ifdef MONITOR_STATIC
extern pthread_create_fcn_t  __real_pthread_create;
#ifdef MONITOR_USE_SIGNALS
extern sigaction_fcn_t    __real_sigaction;
extern sigprocmask_fcn_t  __real_pthread_sigmask;
#endif
#endif

static pthread_create_fcn_t  *real_pthread_create;
static pthread_equal_fcn_t   *real_pthread_equal;
static pthread_key_create_fcn_t   *real_pthread_key_create;
static pthread_key_delete_fcn_t   *real_pthread_key_delete;
static pthread_kill_fcn_t  *real_pthread_kill;
static pthread_self_fcn_t  *real_pthread_self;
static pthread_mutex_lock_fcn_t  *real_pthread_mutex_lock;
static pthread_mutex_lock_fcn_t  *real_pthread_mutex_unlock;
static pthread_getspecific_fcn_t  *real_pthread_getspecific;
static pthread_setspecific_fcn_t  *real_pthread_setspecific;
static pthread_cleanup_push_fcn_t  *real_pthread_cleanup_push;
static pthread_cleanup_pop_fcn_t   *real_pthread_cleanup_pop;
static sigaction_fcn_t    *real_sigaction;
static sigprocmask_fcn_t  *real_pthread_sigmask;

/*
 *  The global thread mutex protects monitor's list of threads and
 *  related state variables.  The values inside a single node don't
 *  really need a mutex.
 *
 *  Note: After monitor begins exit cleanup, then we don't insert or
 *  delete any new thread nodes into the list.  Instead, we set the
 *  tn_* variables to indicate the thread status and when it has
 *  finished.
 */
static pthread_mutex_t monitor_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MONITOR_THREAD_LOCK    (*real_pthread_mutex_lock)(&monitor_thread_mutex)
#define MONITOR_THREAD_UNLOCK  (*real_pthread_mutex_unlock)(&monitor_thread_mutex)

static LIST_HEAD(, monitor_thread_node) monitor_thread_list;

volatile static int  monitor_thread_num = 0;
volatile static char monitor_end_process_cookie = 0;
volatile static char monitor_in_exit_cleanup = 0;

/***  End of mutex-protected variables.  ***/

static pthread_key_t monitor_pthread_key;

volatile static char monitor_has_used_threads = 0;
volatile static char monitor_has_reached_main = 0;
volatile static char monitor_thread_support_done = 0;

extern void monitor_thread_fence1;
extern void monitor_thread_fence2;
extern void monitor_thread_fence3;
extern void monitor_thread_fence4;

/*
 *----------------------------------------------------------------------
 *  INTERNAL THREAD FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Note: there is a tiny window where a thread exists but its thread-
 *  specific data is not yet set, so it's not really an error for
 *  getspecific to fail.  But bad magic implies an internal error.
 */
static struct monitor_thread_node *
monitor_get_tn(void)
{
    struct monitor_thread_node *tn;

    if (monitor_has_used_threads) {
	tn = (*real_pthread_getspecific)(monitor_pthread_key);
	if (tn != NULL && tn->tn_magic != MONITOR_TN_MAGIC) {
	    MONITOR_WARN("bad magic in thread node: %p\n", tn);
	    tn = NULL;
	}
    } else
	tn = monitor_get_main_tn();

    return (tn);
}

static void
monitor_thread_name_init(void)
{
    MONITOR_RUN_ONCE(thread_name_init);

    MONITOR_GET_REAL_NAME_WRAP(real_pthread_create, pthread_create);
#ifdef MONITOR_PTHREAD_EQUAL_IS_FCN
    MONITOR_GET_REAL_NAME(real_pthread_equal, pthread_equal);
#endif
    MONITOR_GET_REAL_NAME(real_pthread_key_create, pthread_key_create);
    MONITOR_GET_REAL_NAME(real_pthread_key_delete, pthread_key_delete);
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
#ifdef MONITOR_USE_SIGNALS
    MONITOR_GET_REAL_NAME_WRAP(real_sigaction, sigaction);
    MONITOR_GET_REAL_NAME_WRAP(real_pthread_sigmask, pthread_sigmask);
#else
    MONITOR_GET_REAL_NAME(real_sigaction, sigaction);
    MONITOR_GET_REAL_NAME(real_pthread_sigmask, pthread_sigmask);
#endif
}

/*
 *  Run in the main thread on the first call to pthread_create(),
 *  before any new threads are created.  Only called once from the
 *  pthread_create() override.
 */
static void
monitor_thread_list_init(void)
{
    struct monitor_thread_node *main_tn;
    int ret;

    /*
     * Give main.c functions a chance to initialize in the case that
     * some library calls pthread_create() before main().
     */
    monitor_early_init();
    monitor_thread_name_init();

    MONITOR_DEBUG1("\n");
    LIST_INIT(&monitor_thread_list);

    ret = (*real_pthread_key_create)(&monitor_pthread_key, NULL);
    if (ret != 0) {
	MONITOR_ERROR("pthread_key_create failed (%d)\n", ret);
    }
    /*
     * main_tn's thread-specific data.
     */
    main_tn = monitor_get_main_tn();
    if (main_tn == NULL || main_tn->tn_magic != MONITOR_TN_MAGIC) {
	MONITOR_ERROR1("monitor_get_main_tn failed\n");
    }
    main_tn->tn_self = pthread_self();
    ret = (*real_pthread_setspecific)(monitor_pthread_key, main_tn);
    if (ret != 0) {
	MONITOR_ERROR("pthread_setspecific failed (%d)\n", ret);
    }
}

/*
 *  Reset the thread list in the child after fork().
 *
 *  If a threaded program forks, then the child has only one running
 *  thread.  So, reset the thread node for the main thread, and free
 *  the thread list and the pthread key.
 */
void
monitor_reset_thread_list(struct monitor_thread_node *main_tn)
{
    struct monitor_thread_node *tn, *tn2;

    if (! monitor_has_used_threads)
	return;

    MONITOR_DEBUG1("\n");
    /*
     * The thread that fork()ed is now the main thread.
     */
    tn = monitor_get_tn();
    if (tn == NULL) {
	MONITOR_WARN1("tn should not be NULL here\n");
    }
    else if (tn != main_tn) {
	memset(main_tn, 0, sizeof(struct monitor_thread_node));
	main_tn->tn_magic = MONITOR_TN_MAGIC;
	main_tn->tn_tid = 0;
	main_tn->tn_user_data = tn->tn_user_data;
	main_tn->tn_stack_bottom = tn->tn_stack_bottom;
	main_tn->tn_is_main = 1;
    }
    /*
     * Free the thread list and the pthread key.
     */
    tn = LIST_FIRST(&monitor_thread_list);
    while (tn != NULL) {
	tn2 = LIST_NEXT(tn, tn_links);
	free(tn);
	tn = tn2;
    }
    if ((*real_pthread_key_delete)(monitor_pthread_key) != 0) {
	MONITOR_WARN1("pthread_key_delete failed\n");
    }	
    monitor_has_used_threads = 0;
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
    MONITOR_THREAD_LOCK;
    tn->tn_tid = ++monitor_thread_num;
    MONITOR_THREAD_UNLOCK;

    return (tn);
}

/*
 *  Returns: 0 on success, or 1 if at exit cleanup and thus we don't
 *  allow any new threads.
 */
static int
monitor_link_thread_node(struct monitor_thread_node *tn)
{
    MONITOR_THREAD_LOCK;
    if (monitor_in_exit_cleanup) {
	MONITOR_THREAD_UNLOCK;
	return (1);
    }
    LIST_INSERT_HEAD(&monitor_thread_list, tn, tn_links);
    MONITOR_THREAD_UNLOCK;
    return (0);
}

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
	MONITOR_WARN1("pthread_getspecific failed: "
		      "unable to call monitor_fini_thread\n");
	return;
    }
    if (tn->tn_appl_started && !tn->tn_fini_started) {
	tn->tn_fini_started = 1;
	MONITOR_DEBUG("calling monitor_fini_thread(data = %p), tid = %d ...\n",
		      tn->tn_user_data, tn->tn_tid);
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
 *  Broadcast specified signal to all threads except self.
 */
void
monitor_broadcast_signal (int signum)
{
    struct monitor_thread_node *tn, *my_tn;
    pthread_t self;

    MONITOR_DEBUG("in monitor broadcast signal %d\n", signum);

    if (! monitor_has_used_threads) {
	MONITOR_DEBUG1("(no threads)\n");
	return;
    }

    MONITOR_THREAD_LOCK;
    /*
     * Walk through the list of threads and send a signum signal to 
     * each one.
     */
    self = (*real_pthread_self)();
    my_tn = NULL;
    for (tn = LIST_FIRST(&monitor_thread_list);
         tn != NULL;
         tn = LIST_NEXT(tn, tn_links)) {

       if (PTHREAD_EQUAL(self, tn->tn_self)) {
		my_tn = tn;
		continue;
       }
       if (tn->tn_appl_started && !tn->tn_fini_started)
          (*real_pthread_kill)(tn->tn_self, signum);
    }
    MONITOR_THREAD_UNLOCK;
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

    if (! monitor_has_used_threads) {
	MONITOR_DEBUG1("(no threads)\n");
	return;
    }

    MONITOR_THREAD_LOCK;
    monitor_in_exit_cleanup = 1;
    MONITOR_THREAD_UNLOCK;
    MONITOR_DEBUG1("(threads)\n");

    /*
     * Install the signal handler for MONITOR_EXIT_CLEANUP_SIGNAL.
     * Note: the signal handler is process-wide.
     */
    sigemptyset(&empty_set);
    my_action.sa_handler = monitor_thread_signal_handler;
    my_action.sa_mask = empty_set;
    my_action.sa_flags = 0;
    if ((*real_sigaction)(MONITOR_EXIT_CLEANUP_SIGNAL,
			  &my_action, NULL) != 0) {
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
	my_tn->tn_fini_started = 1;
	MONITOR_DEBUG("calling monitor_fini_thread(data = %p), tid = %d ...\n",
		      my_tn->tn_user_data, my_tn->tn_tid);
	monitor_fini_thread(my_tn->tn_user_data);
	my_tn->tn_fini_done = 1;
    }
}

/*
 *  If multiple threads try to exit the process simultaneously, then
 *  the first one wins and carries out the process shutdown functions,
 *  and the others wait.  The winner is the thread that flips the
 *  monitor_end_process_cookie bit.
 *
 *  Note: monitor catches process exit in several places, so it's
 *  possible for a single thread to get here multiple times, even in a
 *  non-threaded program.
 *
 *  Returns: 1 if this thread wins the race, else 0.
 */
int
monitor_end_process_race(void)
{
    int ans;

    if (monitor_has_used_threads) {
	MONITOR_THREAD_LOCK;
    }
    ans = !monitor_end_process_cookie;
    monitor_end_process_cookie = 1;
    if (monitor_has_used_threads) {
	MONITOR_THREAD_UNLOCK;
    }

    MONITOR_DEBUG("ans = %d\n", ans);
    return (ans);
}

/*
 *----------------------------------------------------------------------
 *  CLIENT SUPPORT FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Returns: 1 if the application has called pthread_create().
 */
int
monitor_is_threaded(void)
{
    return (monitor_has_used_threads);
}

/*
 *  Returns: the client's data pointer from monitor_init_process() or
 *  monitor_init_thread(), or else NULL on error.
 */
void *
monitor_get_user_data(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    if (tn == NULL) {
	MONITOR_DEBUG1("unable to find thread node\n");
	return (NULL);
    }
    return (tn->tn_user_data);
}

/*
 *  Returns: the calling thread's tid number, or else -1 on error.
 */
int
monitor_get_thread_num(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    return (tn == NULL) ? -1 : tn->tn_tid;
}

/*
 *  Returns: 1 if address is at the bottom of the thread's call stack,
 *  else 0.
 */
int
monitor_unwind_thread_bottom_frame(void *addr)
{
    return (&monitor_thread_fence1 <= addr && addr <= &monitor_thread_fence4);
}

/*
 *  Returns: an address inside the thread's bottom stack frame,
 *  or else NULL if an error occurred.
 */
void *
monitor_stack_bottom(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    if (tn == NULL) {
	MONITOR_DEBUG1("unable to find thread node\n");
	return (NULL);
    }
    return (tn->tn_stack_bottom);
}

/*
 *  Returns: 1 if address is anywhere within the function body of
 *  monitor_main() or monitor_begin_thread().
 */
int
monitor_in_start_func_wide(void *addr)
{
    return monitor_in_main_start_func_wide(addr) ||
	(&monitor_thread_fence1 <= addr && addr <= &monitor_thread_fence4);
}

/*
 *  Returns: 1 if address is within the function body of monitor_main()
 *  or monitor_begin_thread() at the point where it calls the
 *  application.
 */
int
monitor_in_start_func_narrow(void *addr)
{
    return monitor_in_main_start_func_narrow(addr) ||
	(&monitor_thread_fence2 <= addr && addr <= &monitor_thread_fence3);
}

/*
 *  Client access to the real pthread_sigmask().
 */
int
monitor_real_pthread_sigmask(int how, const sigset_t *set,
			     sigset_t *oldset)
{
    monitor_thread_name_init();
    return (*real_pthread_sigmask)(how, set, oldset);
}

/*
 *----------------------------------------------------------------------
 *  PTHREAD_CREATE OVERRIDE and HELPER FUNCTIONS
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

    if (tn == NULL || tn->tn_magic != MONITOR_TN_MAGIC) {
	MONITOR_WARN1("missing thread-specific data\n");
	MONITOR_DEBUG1("calling monitor_fini_thread(NULL)\n");
	monitor_fini_thread(NULL);
	return;
    }

    if (! tn->tn_fini_started) {
	tn->tn_fini_started = 1;
	MONITOR_DEBUG("calling monitor_fini_thread(data = %p), tid = %d ...\n",
		      tn->tn_user_data, tn->tn_tid);
	monitor_fini_thread(tn->tn_user_data);
	tn->tn_fini_done = 1;
    } else {
	MONITOR_DEBUG("already called fini thread for tid = %d\n", tn->tn_tid);
    }
    monitor_unlink_thread_node(tn);
}

/*
 *  Called from real_pthread_create(), it's where the newly-created
 *  thread begins.
 */
static void *
monitor_begin_thread(void *arg)
{
    struct monitor_thread_node *tn = arg;
    void *ret;

    MONITOR_ASM_LABEL(monitor_thread_fence1);
    MONITOR_DEBUG("tid = %d\n", tn->tn_tid);

    /*
     * Don't create any new threads after someone has called exit().
     */
    if (monitor_link_thread_node(tn) != 0) {
	MONITOR_WARN1("trying to create new thread during exit cleanup: "
		      "thread not started\n");
	return (NULL);
    }
    tn->tn_self = (*real_pthread_self)();
    tn->tn_stack_bottom = alloca(8);
    strncpy(tn->tn_stack_bottom, "stakbot", 8);
    if ((*real_pthread_setspecific)(monitor_pthread_key, tn) != 0) {
	MONITOR_ERROR1("pthread_setspecific failed\n");
    }

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

    MONITOR_DEBUG("calling monitor_init_thread(tid = %d, data = %p) ...\n",
		  tn->tn_tid, tn->tn_user_data);
    tn->tn_user_data = monitor_init_thread(tn->tn_tid, tn->tn_user_data);

    PTHREAD_CLEANUP_PUSH(monitor_pthread_cleanup_routine, tn);

    tn->tn_appl_started = 1;
    MONITOR_ASM_LABEL(monitor_thread_fence2);
    ret = (tn->tn_start_routine)(tn->tn_arg);
    MONITOR_ASM_LABEL(monitor_thread_fence3);

    PTHREAD_CLEANUP_POP(1);

    MONITOR_ASM_LABEL(monitor_thread_fence4);
    return (ret);
}

/*
 *  Override pthread_create().
 */
int
MONITOR_WRAP_NAME(pthread_create) (PTHREAD_CREATE_PARAM_LIST)
{
    struct monitor_thread_node *tn;
    int first, ret;

    /*
     * There is no race condition to get here first because until now,
     * there is only one thread.
     */
    first = !monitor_has_used_threads;
    if (first) {
	monitor_thread_list_init();
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
    MONITOR_DEBUG1("calling monitor_thread_pre_create() ...\n");
    tn->tn_user_data = monitor_thread_pre_create();

    ret = (*real_pthread_create)
	(thread, attr, monitor_begin_thread, (void *)tn);

    MONITOR_DEBUG1("calling monitor_thread_post_create() ...\n");
    monitor_thread_post_create(tn->tn_user_data);

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

/*
 *  Allow the application to modify the thread signal mask, but don't
 *  let it block a signal that the client catches.
 */
#ifdef MONITOR_USE_SIGNALS
int
MONITOR_WRAP_NAME(pthread_sigmask)(int how, const sigset_t *set,
				   sigset_t *oldset)
{
    sigset_t my_set;

    monitor_signal_init();
    monitor_thread_name_init();

    if (set != NULL && (how == SIG_BLOCK || how == SIG_SETMASK)) {
	my_set = *set;
	monitor_remove_client_signals(&my_set);
	set = &my_set;
    }

    return (*real_pthread_sigmask)(how, set, oldset);
}
#endif
