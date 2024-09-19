// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor pthread functions.
 *
 *  Override functions:
 *
 *    pthread_create
 *    pthread_exit
 *    pthread_sigmask
 *    sigwait
 *    sigwaitinfo
 *    sigtimedwait
 *
 *  Support functions:
 *
 *    monitor_is_threaded
 *    monitor_get_user_data
 *    monitor_get_thread_num
 *    monitor_stack_bottom
 *    monitor_in_start_func_wide
 *    monitor_in_start_func_narrow
 *    monitor_broadcast_signal
 *    monitor_get_addr_thread_start
 */

#define _GNU_SOURCE

#include <sys/time.h>
#include <sys/types.h>
#include <alloca.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include "common.h"
#include "monitor.h"
#include "pthread_h.h"
#include "queue.h"
#include "spinlock.h"
#include "../audit/audit-api.h"

/*
 *----------------------------------------------------------------------
 *  GLOBAL VARIABLES and EXTERNAL DECLARATIONS
 *----------------------------------------------------------------------
 */

#define MONITOR_TN_ARRAY_SIZE  600
#define MONITOR_SHOOTDOWN_TIMEOUT  10

/*
 *  On some systems, pthread_equal() and pthread_cleanup_push/pop()
 *  are macros and sometimes they're library functions.
 *  NB: We don't actually override these symbols, so we don't care whether they're functions or
 *  macros, we can call them all the same like so.
 */
#define PTHREAD_EQUAL(t1, t2)  pthread_equal((t1), (t2))
#define PTHREAD_CLEANUP_PUSH(fcn, arg)  pthread_cleanup_push((fcn), (arg))
#define PTHREAD_CLEANUP_POP(arg)        pthread_cleanup_pop(arg)

/*
 *  External functions.
 */
#define PTHREAD_CREATE_PARAM_LIST               \
    pthread_t *thread,                          \
    const pthread_attr_t *attr,                 \
    pthread_start_fcn_t *start_routine,         \
    void *arg

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
#define MONITOR_THREAD_LOCK     spinlock_lock(&monitor_thread_lock)
#define MONITOR_THREAD_TRYLOCK  spinlock_trylock(&monitor_thread_lock)
#define MONITOR_THREAD_UNLOCK   spinlock_unlock(&monitor_thread_lock)

static spinlock_t monitor_thread_lock = SPINLOCK_UNLOCKED;

static struct monitor_thread_node monitor_init_tn_array[MONITOR_TN_ARRAY_SIZE];
static struct monitor_thread_node *monitor_tn_array = &monitor_init_tn_array[0];
static int monitor_tn_array_pos = 0;

static LIST_HEAD(, monitor_thread_node) monitor_thread_list;
static LIST_HEAD(, monitor_thread_node) monitor_free_list;

volatile static int  monitor_thread_num = 0;
volatile static char monitor_in_exit_cleanup = 0;

/***  End of mutex-protected variables.  ***/

static pthread_key_t monitor_pthread_key;

volatile static char monitor_has_used_threads = 0;
volatile static char monitor_thread_support_done = 0;
volatile static char monitor_fini_thread_done = 0;
static int shootdown_signal = 0;

extern char monitor_thread_fence1;
extern char monitor_thread_fence2;
extern char monitor_thread_fence3;
extern char monitor_thread_fence4;

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
struct monitor_thread_node *
monitor_get_tn(void)
{
    struct monitor_thread_node *tn;

    if (monitor_has_used_threads) {
        tn = pthread_getspecific(monitor_pthread_key);
        if (tn != NULL && tn->tn_magic != MONITOR_TN_MAGIC) {
            MONITOR_WARN_NO_TID("bad magic in thread node: %p\n", tn);
            tn = NULL;
        }
    } else {
        tn = monitor_get_main_tn();
    }

    return (tn);
}

static void
monitor_thread_name_init(void)
{
    MONITOR_RUN_ONCE(thread_name_init);
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
    LIST_INIT(&monitor_free_list);
    monitor_tn_array_pos = 0;

    ret = pthread_key_create(&monitor_pthread_key, NULL);
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
    main_tn->tn_self = auditor_exports()->pthread_self();
    ret = pthread_setspecific(monitor_pthread_key, main_tn);
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
    struct monitor_thread_node *tn;

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
     * Free the thread list and the pthread key.  Technically, this
     * could leak memory, but only if the process spawns more than 150
     * threads before fork()ing.
     */
    LIST_INIT(&monitor_thread_list);
    LIST_INIT(&monitor_free_list);
    monitor_tn_array_pos = 0;
    if (pthread_key_delete(monitor_pthread_key) != 0) {
        MONITOR_WARN1("pthread_key_delete failed\n");
    }
    monitor_has_used_threads = 0;
}

/*
 *  Try in order: (1) the free list, (2) the pre-allocated tn array,
 *  (3) malloc a new tn array.
 */
static struct monitor_thread_node *
monitor_make_thread_node(void)
{
    struct monitor_thread_node *tn;

    MONITOR_THREAD_LOCK;

    tn = LIST_FIRST(&monitor_free_list);
    if (tn != NULL) {
        /* Use the head of the free list. */
        LIST_REMOVE(tn, tn_links);
    }
    else if (monitor_tn_array_pos < MONITOR_TN_ARRAY_SIZE) {
        /* Use the next element in the tn array. */
        tn = &monitor_tn_array[monitor_tn_array_pos];
        monitor_tn_array_pos++;
    }
    else {
        /* Malloc a new tn array. */
        monitor_tn_array = malloc(MONITOR_TN_ARRAY_SIZE * sizeof(struct monitor_thread_node));
        if (monitor_tn_array == NULL) {
            MONITOR_ERROR1("malloc failed\n");
        }
        tn = &monitor_tn_array[0];
        monitor_tn_array_pos = 1;
    }

    memset(tn, 0, sizeof(struct monitor_thread_node));
    tn->tn_magic = MONITOR_TN_MAGIC;
    tn->tn_tid = -1;

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
    tn->tn_tid = ++monitor_thread_num;
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
    if (monitor_in_exit_cleanup) {
        tn->tn_fini_started = 1;
        tn->tn_fini_done = 1;
    } else {
        LIST_REMOVE(tn, tn_links);
        memset(tn, 0, sizeof(struct monitor_thread_node));
        LIST_INSERT_HEAD(&monitor_free_list, tn, tn_links);
    }
    MONITOR_THREAD_UNLOCK;
}

static void
monitor_shootdown_handler(int sig)
{
    struct monitor_thread_node *tn;
    int old_state;

    tn = pthread_getspecific(monitor_pthread_key);
    if (tn == NULL) {
        MONITOR_WARN1("unable to deliver monitor_fini_thread callback: "
                      "pthread_getspecific() failed\n");
        return;
    }
    if (tn->tn_magic != MONITOR_TN_MAGIC) {
        MONITOR_WARN1("unable to deliver monitor_fini_thread callback: "
                      "bad magic in thread node\n");
        return;
    }
    if (!tn->tn_appl_started || tn->tn_fini_started || tn->tn_block_shootdown) {
        /* fini-thread has already run, or else we don't want it to run. */
        return;
    }
    if (monitor_fini_thread_done) {
        MONITOR_WARN("unable to deliver monitor_fini_thread callback (tid %d): "
                     "monitor_fini_process() has begun\n", tn->tn_tid);
        return;
    }

    auditor_exports()->pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
    tn->tn_fini_started = 1;
    MONITOR_DEBUG("calling monitor_fini_thread(data = %p), tid = %d ...\n",
                  tn->tn_user_data, tn->tn_tid);
    monitor_fini_thread(tn->tn_user_data);
    tn->tn_fini_done = 1;
    auditor_exports()->pthread_setcancelstate(old_state, NULL);
}


/*
 *  Called from main.c at end process time for possible thread cleanup.
 *  Note: main doesn't know if application is threaded or not.
 */
void
monitor_thread_shootdown(void)
{
    struct timeval last, now;
    struct monitor_thread_node *tn, *my_tn, *main_tn;
    struct sigaction my_action;
    sigset_t empty_set;
    pthread_t self;
    int num_started, num_unstarted, last_started;
    int num_finished, num_unfinished, old_state;

    if (! monitor_has_used_threads) {
        MONITOR_DEBUG1("(no threads)\n");
        return;
    }

    auditor_exports()->pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);

    MONITOR_THREAD_LOCK;
    monitor_in_exit_cleanup = 1;
    MONITOR_THREAD_UNLOCK;
    MONITOR_DEBUG1("(threads)\n");

    /*
     * Install the signal handler for thread shootdown.
     * Note: the signal handler is process-wide.
     */
    shootdown_signal = monitor_shootdown_signal();
    MONITOR_DEBUG("using signal: %d\n", shootdown_signal);
    sigemptyset(&empty_set);
    my_action.sa_handler = monitor_shootdown_handler;
    my_action.sa_mask = empty_set;
    my_action.sa_flags = SA_RESTART;
    if (auditor_exports()->sigaction(shootdown_signal, &my_action, NULL) != 0) {
        MONITOR_ERROR1("sigaction failed\n");
    }

    /*
     * If shootdown is called from non-main thread, then cheat and put
     * main on the thread list.  Main gets a fini-thread callback and
     * the current thread gets both fini-thread and fini-process.
     */
    self = auditor_exports()->pthread_self();
    main_tn = monitor_get_main_tn();
    if (! PTHREAD_EQUAL(self, main_tn->tn_self)) {
        main_tn->tn_appl_started = 1;
        main_tn->tn_fini_started = 0;
        main_tn->tn_fini_done = 0;
        LIST_INSERT_HEAD(&monitor_thread_list, main_tn, tn_links);
    }

    /*
     * Walk through the list of unfinished threads, send a signal to
     * force them into their fini_thread functions, and wait until
     * they all finish.  But don't signal ourself.
     *
     * Add a timeout: if we make no progress for 10 consecutive
     * seconds, then give up.  Progress means receiving the signal in
     * the other thread, the fini thread callback can take as long as
     * it likes.
     */
    my_tn = NULL;
    gettimeofday(&last, NULL);
    last_started = 0;
    for (;;) {
        num_started = 0;
        num_unstarted = 0;
        num_finished = 0;
        num_unfinished = 0;
        for (tn = LIST_FIRST(&monitor_thread_list);
             tn != NULL;
             tn = LIST_NEXT(tn, tn_links))
        {
            if (PTHREAD_EQUAL(self, tn->tn_self)) {
                my_tn = tn;
                continue;
            }
            if (tn->tn_appl_started) {
                if (tn->tn_fini_started) {
                    num_started++;
                } else {
                    auditor_exports()->pthread_kill(tn->tn_self, shootdown_signal);
                    num_unstarted++;
                }
                if (tn->tn_fini_done)
                    num_finished++;
                else
                    num_unfinished++;
            }
        }
        MONITOR_DEBUG("started: %d, unstarted: %d, finished: %d, unfinished: %d\n",
                      num_started, num_unstarted, num_finished, num_unfinished);
        if (num_unfinished == 0)
            break;

        gettimeofday(&now, NULL);
        if (num_started > last_started) {
            last = now;
            last_started = num_started;
        } else if (now.tv_sec > last.tv_sec + MONITOR_SHOOTDOWN_TIMEOUT
                   && num_unstarted > 0) {
            MONITOR_WARN("timeout exceeded (%d): unable to deliver "
                         "monitor_fini_thread() to %d threads\n",
                         MONITOR_SHOOTDOWN_TIMEOUT, num_unstarted);
            break;
        }
        usleep(MONITOR_POLL_USLEEP_TIME);
    }
    monitor_fini_thread_done = 1;

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

    auditor_exports()->pthread_setcancelstate(old_state, NULL);
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
 *  Returns: the address of the pthread start routine for the current
 *  thread, or else NULL on error.
 */
void *
monitor_get_addr_thread_start(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    return (tn == NULL) ? NULL : tn->tn_start_routine;
}

/*
 *  Returns: 1 if address is at the bottom of the thread's call stack,
 *  else 0.
 */
int
monitor_unwind_thread_bottom_frame(void *addr)
{
    return (&monitor_thread_fence1 <= (char *) addr
            && (char *) addr <= &monitor_thread_fence4);
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
        (&monitor_thread_fence1 <= (char *) addr
         && (char *) addr <= &monitor_thread_fence4);
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
        (&monitor_thread_fence2 <= (char *) addr
         && (char *) addr <= &monitor_thread_fence3);
}

/*
 *  Send signal 'sig' to every thread except ourself.  Note: we call
 *  this function from a signal handler, so to avoid deadlock, we
 *  can't wait on a lock.  Instead, we can only try the lock and fail
 *  if the trylock fails.
 *
 *  Returns: 0 on success, or -1 if trylock failed.
 */
int
monitor_broadcast_signal(int sig)
{
    struct monitor_thread_node *tn;
    pthread_t self;

    if (! monitor_has_used_threads)
        return (SUCCESS);

    if (MONITOR_THREAD_TRYLOCK != 0)
        return (FAILURE);

    self = auditor_exports()->pthread_self();
    for (tn = LIST_FIRST(&monitor_thread_list);
         tn != NULL;
         tn = LIST_NEXT(tn, tn_links)) {

        if (!PTHREAD_EQUAL(self, tn->tn_self) &&
            tn->tn_appl_started && !tn->tn_fini_started) {
            auditor_exports()->pthread_kill(tn->tn_self, sig);
        }
    }

    tn = monitor_get_main_tn();
    if (tn != NULL && !PTHREAD_EQUAL(self, tn->tn_self))
        auditor_exports()->pthread_kill(tn->tn_self, sig);

    MONITOR_THREAD_UNLOCK;
    return (SUCCESS);
}

/*
 *  Block and unblock receiving a thread shootdown signal.  This is
 *  used in hpctoolkit to block receiving the fini-thread callback
 *  (via shootdown signal) while in the middle of a sample.
 *
 *  Returns: 1 if already at process exit.
 */
int
monitor_block_shootdown(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    if (tn == NULL) {
        MONITOR_DEBUG1("unable to find thread node\n");
        return 0;
    }
    tn->tn_block_shootdown = 1;

    return monitor_in_exit_cleanup;
}

void
monitor_unblock_shootdown(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    if (tn == NULL) {
        MONITOR_DEBUG1("unable to find thread node\n");
        return;
    }
    tn->tn_block_shootdown = 0;
}

/*
 *  Allow the client to ignore some new threads.  This is mostly
 *  useful for library calls that we call that create new threads.
 *  PAPI cuda does this.
 */
void
monitor_disable_new_threads(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    if (tn == NULL) {
        MONITOR_DEBUG1("unable to find thread node\n");
        return;
    }
    tn->tn_ignore_threads = 1;
}

void
monitor_enable_new_threads(void)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    if (tn == NULL) {
        MONITOR_DEBUG1("unable to find thread node\n");
        return;
    }
    tn->tn_ignore_threads = 0;
}

/*
 *  Copy pthread_create()'s thread info struct into mti.  Note: this
 *  info is only available inside pthread_create().
 *
 *  Returns: 0 on success, or else 1 if error or if called from
 *  outside pthread_create().
 */
int
monitor_get_new_thread_info(struct monitor_thread_info *mti)
{
    struct monitor_thread_node *tn;

    if (mti == NULL) {
        return 1;
    }

    tn = monitor_get_tn();
    if (tn == NULL) {
        MONITOR_DEBUG1("unable to find thread node\n");
        return 1;
    }

    /* Called from outside pthread_create(). */
    if (tn->tn_thread_info == NULL) {
        return 1;
    }

    memcpy(mti, tn->tn_thread_info, sizeof(struct monitor_thread_info));

    return 0;
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

    if (tn == NULL) {
        MONITOR_WARN1("unable to deliver monitor_fini_thread callback: "
                      "missing cleanup handler argument\n");
        return;
    }
    if (tn->tn_magic != MONITOR_TN_MAGIC) {
        MONITOR_WARN1("unable to deliver monitor_fini_thread callback: "
                      "bad magic in thread node\n");
        return;
    }
    if (!tn->tn_appl_started || tn->tn_fini_started || tn->tn_block_shootdown) {
        /* fini-thread has already run, or else we don't want it to run. */
        return;
    }
    if (monitor_fini_thread_done) {
        MONITOR_WARN("unable to deliver monitor_fini_thread callback (tid %d): "
                     "monitor_fini_process() has begun\n", tn->tn_tid);
        return;
    }

    tn->tn_fini_started = 1;
    MONITOR_DEBUG("calling monitor_fini_thread(data = %p), tid = %d ...\n",
                  tn->tn_user_data, tn->tn_tid);
    monitor_fini_thread(tn->tn_user_data);
    tn->tn_fini_done = 1;

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
    MONITOR_DEBUG1("\n");

    /*
     * Don't create any new threads after someone has called exit().
     */
    tn->tn_self = auditor_exports()->pthread_self();
    tn->tn_stack_bottom = alloca(8);
    strncpy(tn->tn_stack_bottom, "stakbot", 8);
    if (pthread_setspecific(monitor_pthread_key, tn) != 0) {
        MONITOR_ERROR1("pthread_setspecific failed\n");
    }
    if (monitor_link_thread_node(tn) != 0) {
        MONITOR_DEBUG1("warning: trying to create new thread during "
                       "exit cleanup: thread not started\n");
        return (NULL);
    }

    PTHREAD_CLEANUP_PUSH(monitor_pthread_cleanup_routine, tn);

    MONITOR_DEBUG("tid = %d, self = %p, start_routine = %p\n",
                  tn->tn_tid, (void *)tn->tn_self, tn->tn_start_routine);
    MONITOR_DEBUG("calling monitor_init_thread(tid = %d, data = %p) ...\n",
                  tn->tn_tid, tn->tn_user_data);
    tn->tn_user_data = monitor_init_thread(tn->tn_tid, tn->tn_user_data);

    tn->tn_appl_started = 1;
    MONITOR_ASM_LABEL(monitor_thread_fence2);
    ret = (tn->tn_start_routine)(tn->tn_arg);
    MONITOR_ASM_LABEL(monitor_thread_fence3);

    PTHREAD_CLEANUP_POP(1);

    MONITOR_ASM_LABEL(monitor_thread_fence4);
    return (ret);
}

/*
 *  Allow the client to change the thread stack size, if needed.  This
 *  is useful for profilers, in case the application's stack is too
 *  small.  This would be easier if we could copy the attribute object
 *  and modify the copy, but that violates the pthread spec (not
 *  allowed to copy).
 *
 *  Returns: pointer to attr object that pthread_create() should use,
 *  also sets the old size, whether we need to restore the old size,
 *  and whether we need to destroy the attr object.  If anything goes
 *  wrong, use the original attributes and hope for the best.
 */
pthread_attr_t *
monitor_adjust_stack_size(pthread_attr_t *orig_attr,
                          pthread_attr_t *default_attr,
                          int *restore, int *destroy, size_t *old_size,
                          const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    pthread_attr_t *attr;
    size_t new_size;

    *restore = 0;
    *destroy = 0;
    if (orig_attr != NULL)
        attr = orig_attr;
    else {
        if (f_pthread_attr_init(default_attr, dispatch) != 0) {
            MONITOR_WARN1("pthread_attr_init failed\n");
            return (orig_attr);
        }
        *destroy = 1;
        attr = default_attr;
    }

    if (f_pthread_attr_getstacksize(attr, old_size, dispatch) != 0) {
        MONITOR_WARN1("pthread_attr_getstacksize failed\n");
        return (orig_attr);
    }

    new_size = monitor_reset_stacksize(*old_size);
    if (new_size == *old_size)
        return (orig_attr);

    if (f_pthread_attr_setstacksize(attr, new_size, dispatch) != 0) {
        MONITOR_WARN1("pthread_attr_setstacksize failed\n");
        return (orig_attr);
    }
    if (attr == orig_attr)
        *restore = 1;

    MONITOR_DEBUG("old size = %ld, new size = %ld\n",
                  (long)*old_size, (long)new_size);

    return (attr);
}

/*
 *  Override pthread_create().
 */
int
hpcrun_pthread_create(PTHREAD_CREATE_PARAM_LIST, void* caller,
                        const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    struct monitor_thread_node *tn, *my_tn;
    struct monitor_thread_info mti;
    pthread_attr_t default_attr;
    int ret, restore, destroy;
    size_t old_size;

    MONITOR_DEBUG1("\n");

    /*
     * There is no race condition to get here first because until now,
     * there is only one thread.
     *
     * Note: monitor should be fully initialized before issuing any
     * callback functions.
     */
    if (! monitor_has_used_threads) {
        monitor_thread_list_init();
        monitor_has_used_threads = 1;
    }

    /*
     * Create a thread info struct for pthread_create() callback
     * function.  Note: this info is only available during the
     * lifetime of the pthread_create() override.
     */
    mti.mti_create_return_addr = caller;
    mti.mti_start_routine = (void *) start_routine;

    my_tn = monitor_get_tn();
    if (my_tn != NULL) {
        my_tn->tn_thread_info = &mti;
    }

    /*
     * This may be the first entry into monitor if pthread_create() is
     * called from a library init ctor before main.
     */
    monitor_begin_process_fcn(NULL, FALSE);

    /*
     * If we are ignoring this thread, then call the real
     * pthread_create(), don't put it on the thread list and don't
     * give any callbacks.
     */
    if (my_tn == NULL || my_tn->tn_ignore_threads) {
        MONITOR_DEBUG("launching ignored thread: start = %p\n", start_routine);
        return f_pthread_create(thread, attr, start_routine, arg, dispatch);
    }

    /*
     * Call thread_support on the first call to pthread_create(), we
     * no longer defer this.
     */
    if (! monitor_thread_support_done) {
        MONITOR_DEBUG1("calling monitor_init_thread_support() ...\n");
        monitor_thread_support_done = 1;
        monitor_init_thread_support();
    }

    /*
     * Always launch the thread.  If we're in pthread_create() too
     * early, then the new thread will spin-wait until init_process
     * and thread_support are done.
     */
    MONITOR_DEBUG("calling monitor_thread_pre_create(start_routine = %p) ...\n",
                  start_routine);
    void * user_data = monitor_thread_pre_create();

    /*
     * Allow the client to ignore this new thread.
     */
    if (user_data == MONITOR_IGNORE_NEW_THREAD) {
        MONITOR_DEBUG("launching ignored thread: start = %p\n", start_routine);
        return f_pthread_create(thread, attr, start_routine, arg, dispatch);
    }

    tn = monitor_make_thread_node();
    tn->tn_start_routine = start_routine;
    tn->tn_arg = arg;
    tn->tn_user_data = user_data;

    /*
     * Allow the client to change the thread stack size.  Note: we
     * need to restore the original size in case the application uses
     * one attribute struct for several threads (so we don't keep
     * increasing its size).
     */
    attr = monitor_adjust_stack_size((pthread_attr_t *)attr, &default_attr,
                                     &restore, &destroy, &old_size, dispatch);

    MONITOR_DEBUG("launching monitored thread: monitor = %p, start = %p\n",
                  monitor_begin_thread, start_routine);
    ret = f_pthread_create(thread, attr, monitor_begin_thread, (void *)tn, dispatch);

    if (restore) {
        f_pthread_attr_setstacksize((pthread_attr_t *)attr, old_size, dispatch);
    }
    if (destroy) {
        f_pthread_attr_destroy(&default_attr, dispatch);
    }
    if (ret != 0) {
        MONITOR_DEBUG("real_pthread_create failed: start_routine = %p, ret = %d\n",
                      start_routine, ret);
    }

    MONITOR_DEBUG("calling monitor_thread_post_create(start_routine = %p) ...\n",
                  start_routine);
    monitor_thread_post_create(tn->tn_user_data);

    /* The thread info struct's lifetime ends here. */
    if (my_tn != NULL) {
        my_tn->tn_thread_info = NULL;
    }

    return (ret);
}

/*
 *  Pthread_exit() from the main thread exits the process, and this
 *  bypasses our other exit-catching methods, so we have to override
 *  it here.
 */
void
hpcrun_pthread_exit(void *data, const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    struct monitor_thread_node *tn;

    tn = monitor_get_tn();
    if (tn != NULL && tn->tn_is_main) {
        MONITOR_DEBUG1("pthread_exit called from main thread\n");
        monitor_end_process_fcn(MONITOR_EXIT_NORMAL);
    }

    f_pthread_exit(data, dispatch);

    /* Never reached, but silence a compiler warning. */
    exit(0);
}

/*
 *  Allow the application to modify the thread signal mask, but don't
 *  let it change the mask for any signal in the keep open list.
 */
int
hpcrun_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset,
                         const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    sigset_t my_set;

    monitor_signal_init();
    monitor_thread_name_init();

    if (set != NULL) {
        MONITOR_DEBUG1("\n");
        my_set = *set;
        monitor_remove_client_signals(&my_set, how);
        set = &my_set;
    }

    return f_pthread_sigmask(how, set, oldset, dispatch);
}

/*
 *----------------------------------------------------------------------
 *  Override sigwait(), sigwaitinfo() and sigtimedwait()
 *----------------------------------------------------------------------
 */

/*
 *  Returns: 1 if we handled the signal (and thus we restart sigwait),
 *  else 0 to pass the signal to the application.
 */
static int
monitor_sigwait_helper(const sigset_t *set, int sig, int sigwait_errno,
                       siginfo_t *info, ucontext_t *context)
{
    struct monitor_thread_node *tn;
    int old_state;

    /*
     * If sigwaitinfo() returned an error, we restart EINTR and pass
     * other errors back to the application.
     */
    if (sig < 0) {
        return (sigwait_errno == EINTR);
    }

    /*
     * End of process shootdown signal.
     */
    tn = monitor_get_tn();
    if (sig == shootdown_signal
        && monitor_in_exit_cleanup
        && !monitor_fini_thread_done
        && tn != NULL
        && tn->tn_appl_started
        && !tn->tn_fini_started
        && !tn->tn_block_shootdown)
    {
        auditor_exports()->pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
        tn->tn_fini_started = 1;
        MONITOR_DEBUG("calling monitor_fini_thread(data = %p), tid = %d ...\n",
                      tn->tn_user_data, tn->tn_tid);
        monitor_fini_thread(tn->tn_user_data);
        tn->tn_fini_done = 1;
        auditor_exports()->pthread_setcancelstate(old_state, NULL);

        return 1;
    }

    /*
     * A signal for which the client has installed a handler via
     * monitor_sigaction().
     */
    if (monitor_sigwait_handler(sig, info, context) == 0) {
        return 1;
    }

    /*
     * A signal not in 'set' is treated as EINTR and restarted.
     */
    if (! sigismember(set, sig)) {
        return 1;
    }

    /*
     * Otherwise, return the signal to the application.
     */
    return 0;
}

int
hpcrun_sigwait(const sigset_t *set, int *sig,
                 const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    char buf[MONITOR_SIG_BUF_SIZE];
    siginfo_t my_info;
    ucontext_t context;
    int ret, save_errno;

    monitor_thread_name_init();
    if (monitor_debug) {
        monitor_sigset_string(buf, MONITOR_SIG_BUF_SIZE, set);
        MONITOR_DEBUG("waiting on:%s\n", buf);
    }

    getcontext(&context);
    do {
        ret = f_sigwaitinfo(set, &my_info, dispatch);
        save_errno = errno;
    }
    while (monitor_sigwait_helper(set, ret, save_errno, &my_info, &context));

    if (ret < 0) {
        return save_errno;
    }
    if (sig != NULL) {
        *sig = ret;
    }
    return 0;
}

int
hpcrun_sigwaitinfo(const sigset_t *set, siginfo_t *info,
                     const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    char buf[MONITOR_SIG_BUF_SIZE];
    siginfo_t my_info, *info_ptr;
    ucontext_t context;
    int ret, save_errno;

    monitor_thread_name_init();
    if (monitor_debug) {
        monitor_sigset_string(buf, MONITOR_SIG_BUF_SIZE, set);
        MONITOR_DEBUG("waiting on:%s\n", buf);
    }

    getcontext(&context);
    info_ptr = (info != NULL) ? info : &my_info;
    do {
        ret = f_sigwaitinfo(set, info_ptr, dispatch);
        save_errno = errno;
    }
    while (monitor_sigwait_helper(set, ret, save_errno, info_ptr, &context));

    errno = save_errno;
    return ret;
}

int
hpcrun_sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout,
                      const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    char buf[MONITOR_SIG_BUF_SIZE];
    siginfo_t my_info, *info_ptr;
    ucontext_t context;
    int ret, save_errno;

    monitor_thread_name_init();
    if (monitor_debug) {
        monitor_sigset_string(buf, MONITOR_SIG_BUF_SIZE, set);
        MONITOR_DEBUG("waiting on:%s\n", buf);
    }

    /*
     * FIXME: if we restart sigtimedwait(), then we should subtract
     * the elapsed time from the timeout.
     */
    getcontext(&context);
    info_ptr = (info != NULL) ? info : &my_info;
    do {
        ret = f_sigtimedwait(set, info_ptr, timeout, dispatch);
        save_errno = errno;
    }
    while (monitor_sigwait_helper(set, ret, save_errno, info_ptr, &context));

    errno = save_errno;
    return ret;
}
