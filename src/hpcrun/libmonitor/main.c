// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor core functions: main and exit.
 *
 *  Override functions:
 *
 *    main, __libc_start_main
 *    exit
 *    _exit, _Exit
 *
 *  Client support functions:
 *
 *    monitor_get_addr_main
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <alloca.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "atomic.h"
#include "monitor.h"
#include "pthread_h.h"

int monitor_debug = 0;

/*
 *----------------------------------------------------------------------
 *  MACROS and GLOBAL VARIABLES
 *----------------------------------------------------------------------
 */

/*
 *  Powerpc uses a different signature for __libc_start_main() and
 *  adds a fourth argument to main().
 */

#ifdef HOST_CPU_PPC

#define AUXVEC_ARG   , auxvec
#define AUXVEC_DECL  , void * auxvec

static void *new_stinfo[4];

#else  /* default __libc_start_main() args */

#define AUXVEC_ARG
#define AUXVEC_DECL

#endif

typedef int start_main_fcn_t(START_MAIN_PARAM_LIST);
typedef int main_fcn_t(int, char **, char **  AUXVEC_DECL );
typedef void exit_fcn_t(int);
typedef int sigprocmask_fcn_t(int, const sigset_t *, sigset_t *);
typedef pid_t fork_fcn_t(void);

static start_main_fcn_t  *real_start_main = NULL;
static main_fcn_t  *real_main = NULL;
static exit_fcn_t  *real_exit = NULL;
static exit_fcn_t  *real_u_exit = NULL;
static sigprocmask_fcn_t *real_sigprocmask = NULL;
static fork_fcn_t  *real_fork = NULL;

static int monitor_argc = 0;
static char **monitor_argv = NULL;
static char **monitor_envp = NULL;

volatile static char monitor_init_library_called = 0;
volatile static char monitor_fini_library_called = 0;
volatile static char monitor_fini_process_done = 0;
volatile static long monitor_end_process_cookie = 0;

extern char monitor_main_fence1;
extern char monitor_main_fence2;
extern char monitor_main_fence3;
extern char monitor_main_fence4;

static struct monitor_thread_node monitor_main_tn;

/*
 *----------------------------------------------------------------------
 *  INIT FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Normally run as part of monitor_normal_init(), but may be run
 *  earlier if someone calls pthread_create() before our library init
 *  function (dynamic case).
 */
void
monitor_early_init(void)
{
    MONITOR_RUN_ONCE(early_init);

    if (! monitor_debug) {
        if (getenv("MONITOR_DEBUG") != NULL)
            monitor_debug = 1;
    }
    MONITOR_DEBUG1("\n");

    memset(&monitor_main_tn, 0, sizeof(struct monitor_thread_node));
    monitor_main_tn.tn_magic = MONITOR_TN_MAGIC;
    monitor_main_tn.tn_tid = 0;
    monitor_main_tn.tn_is_main = 1;
}

/*
 *  Run at library init time (dynamic), or in monitor faux main
 *  (static).
 */
static void
monitor_normal_init(void)
{
    MONITOR_RUN_ONCE(normal_init);

    monitor_early_init();
    MONITOR_DEBUG("%s rev %d\n", "libmonitor", 0);

    /*
     * Always get _exit() first so that we have a way to exit if
     * needed.
     */
    MONITOR_GET_REAL_NAME_WRAP(real_u_exit, _exit);
    MONITOR_GET_REAL_NAME_WRAP(real_exit, exit);
    MONITOR_REQUIRE_DLSYM(real_start_main, "__libc_start_main");

    monitor_fork_init();

    MONITOR_GET_REAL_NAME_WRAP(real_sigprocmask, sigprocmask);
    monitor_signal_init();
}

/*
 *----------------------------------------------------------------------
 *  LIBRARY and PROCESS FUNCTIONS
 *----------------------------------------------------------------------
 */

static void
monitor_begin_library_fcn(void)
{
    MONITOR_RUN_ONCE(begin_library);

    MONITOR_DEBUG1("\n");
    monitor_normal_init();

    MONITOR_DEBUG1("calling monitor_init_library() ...\n");
    monitor_init_library();
    monitor_init_library_called = 1;
}

void
monitor_end_library_fcn(void)
{
    if (monitor_fini_library_called)
        return;

    MONITOR_DEBUG1("calling monitor_fini_library() ...\n");
    monitor_fini_library();
    monitor_fini_library_called = 1;
}

/*
 *  Run the init process callback function on first entry to main(),
 *  fork(), pthread_create() or client entry.
 */
void
monitor_begin_process_fcn(void *user_data, int is_fork)
{
    static long monitor_init_process_called = 0;

    monitor_normal_init();

    /* Always compare and set to 1, in case of fork. */
    long val = compare_and_swap(&monitor_init_process_called, 0, 1);

    if (is_fork) {
        /* Fork() always runs the init process callback.
         */
        monitor_reset_thread_list(&monitor_main_tn);
        monitor_main_tn.tn_user_data = user_data;
    }
    else if (val) {
        /* If already called, then skip the init process callback.
         */
        return;
    }

    monitor_fini_library_called = 0;
    monitor_fini_process_done = 0;

    monitor_begin_library_fcn();

    MONITOR_DEBUG1("calling monitor_init_process() ...\n");
    monitor_main_tn.tn_user_data =
        monitor_init_process(&monitor_argc, monitor_argv, user_data);
}

/*
 *  Monitor catches process exit in several places, so we synchronize
 *  them here.  The first thread to get here invokes the callback
 *  functions, and the others wait for that to finish.
 *
 *  Note: there is a race condition in the system exit() between
 *  _IO_cleanup() and doing output from monitor's library fini
 *  destructor (in debug mode) that can (rarely) result in a segfault.
 *  Here, we avoid the race by delaying the losing threads a few extra
 *  seconds.  But, we can't block them forever because that would
 *  deadlock if the application calls exit() from its own exit
 *  handler.  (It shouldn't do that, but it might.)
 */
void
monitor_end_process_fcn(int how)
{
    struct monitor_thread_node *tn = monitor_get_tn();
    long prev;

    prev = compare_and_swap(&monitor_end_process_cookie, 0, 1);
    if (prev == 0) {
        /*
         * Race winner: use the first thread that starts process exit
         * to launch the fini-thread and fini-process callbacks.
         */
        if (tn != NULL) {
            tn->tn_exit_win = 1;
        }
        MONITOR_DEBUG("calling monitor_begin_process_exit (how = %d) ...\n", how);
        monitor_begin_process_exit(how);

        monitor_thread_shootdown();

        MONITOR_DEBUG("calling monitor_fini_process (how = %d) ...\n", how);
        monitor_fini_process(how, monitor_main_tn.tn_user_data);
    }
    else if (tn != NULL && tn->tn_exit_win) {
        /*
         * Repeat winner: somehow this thread has started process exit
         * again.  Assume the callbacks have had their chance.
         */
        MONITOR_DEBUG("same thread restarting process exit (how = %d)\n", how);
    }
    else {
        /*
         * Race loser: delay this thread until the fini callbacks are
         * finished.
         */
        MONITOR_DEBUG("delay second thread trying to exit (how = %d)\n", how);
        while (! monitor_fini_process_done) {
            usleep(MONITOR_POLL_USLEEP_TIME);
        }
        sleep(2);
    }

    monitor_fini_process_done = 1;
    MONITOR_DEBUG1("resume system exit\n");
}

/*
 *----------------------------------------------------------------------
 *  INTERNAL MONITOR SUPPORT FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Let the other modules see the command-line arguments, if needed.
 *  The Fortran mpi_init() uses this.
 */
void
monitor_get_main_args(int *argc_ptr, char ***argv_ptr, char ***env_ptr)
{
    if (argc_ptr != NULL) {
        *argc_ptr = monitor_argc;
    }
    if (argv_ptr != NULL) {
        *argv_ptr = monitor_argv;
    }
    if (env_ptr != NULL) {
        *env_ptr = monitor_envp;
    }
}

/*
 *  Returns: 1 if address is within the body of the function at the
 *  bottom of the application's call stack, else 0.
 */
int
monitor_unwind_process_bottom_frame(void *addr)
{
    return (&monitor_main_fence1 <= (char *) addr
            && (char *) addr <= &monitor_main_fence4);
}

/*
 *  Returns: 1 if address is anywhere within the function body of
 *  monitor_main().
 */
int
monitor_in_main_start_func_wide(void *addr)
{
    return (&monitor_main_fence1 <= (char *) addr
            && (char *) addr <= &monitor_main_fence4);
}

/*
 *  Returns: 1 if address is within the function body of
 *  monitor_main() at the point where it calls the application.
 */
int
monitor_in_main_start_func_narrow(void *addr)
{
    return (&monitor_main_fence2 <= (char *) addr
            && (char *) addr <= &monitor_main_fence3);
}

/*
 *  Allow the thread functions to access main's thread node struct.
 */
struct monitor_thread_node *
monitor_get_main_tn(void)
{
    return (&monitor_main_tn);
}

/*
 *----------------------------------------------------------------------
 *  CLIENT SUPPORT FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  If the client gains control before libmonitor, then it can call
 *  this to initialize monitor and get an init-process callback.
 */
void
monitor_initialize(void)
{
    monitor_begin_process_fcn(NULL, FALSE);
}

/*
 *  Returns: the address of the application's main() function.
 */
void *
monitor_get_addr_main(void)
{
    return real_main;
}

/*
 *----------------------------------------------------------------------
 *  EXTERNAL OVERRIDES and their helper functions
 *----------------------------------------------------------------------
 */

/*
 *  Static case -- we get into __wrap_main() directly, by editing the
 *  link line so that the system call to main() goes there instead.
 *  In this case, there are no library init/fini functions.
 *
 *  Dynamic case -- we get into __libc_start_main() via LD_PRELOAD.
 */
int
monitor_main(int argc, char **argv, char **envp  AUXVEC_DECL )
{
    int ret;

    MONITOR_ASM_LABEL(monitor_main_fence1);
    monitor_normal_init();
    MONITOR_DEBUG1("\n");
    monitor_argc = argc;
    monitor_argv = argv;
    monitor_envp = envp;

    monitor_main_tn.tn_stack_bottom = alloca(8);
    strncpy(monitor_main_tn.tn_stack_bottom, "stakbot", 8);
    monitor_begin_process_fcn(NULL, FALSE);
    monitor_at_main();

    MONITOR_ASM_LABEL(monitor_main_fence2);
    ret = (*real_main)(argc, argv, envp  AUXVEC_ARG );
    MONITOR_ASM_LABEL(monitor_main_fence3);

    monitor_end_process_fcn(MONITOR_EXIT_NORMAL);

    MONITOR_ASM_LABEL(monitor_main_fence4);
    return (ret);
}

int
foilbase_libc_start_main(START_MAIN_PARAM_LIST)
{
    MONITOR_DEBUG1("\n");
    MONITOR_REQUIRE_DLSYM(real_start_main, "__libc_start_main");

#ifdef HOST_CPU_PPC
    real_main = stinfo[1];
    memcpy(new_stinfo, stinfo, sizeof(new_stinfo));
    new_stinfo[1] = &monitor_main;

    /* Set real_main first, so monitor_get_addr_main() works. */
    monitor_start_main_init();

    (*real_start_main)(argc, argv, envp, auxp, rtld_fini,
                       new_stinfo, stack_end);

#else
    real_main = main;

    monitor_start_main_init();

    (*real_start_main)(monitor_main, argc, argv, init, fini,
                       rtld_fini, stack_end);
#endif

    /* Never reached. */
    return (0);
}

/*
 *  It seems that exit() bypasses the library version of _exit(), plus
 *  the atexit handlers are only run once.  So, we need to override
 *  exit() to handle multiple calls to exit().
 */
void
foilbase_exit(int status)
{
    monitor_normal_init();
    MONITOR_DEBUG1("\n");

    monitor_end_process_fcn(MONITOR_EXIT_NORMAL);
    (*real_exit)(status);

    /* Never reached, but silence a compiler warning. */
    exit(status);
}

/*
 *  _exit and _Exit bypass library fini functions, so we need to call
 *  them here (in the dynamic case).
 */
void
foilbase__exit(int status)
{
    monitor_normal_init();
    MONITOR_DEBUG1("\n");

    monitor_end_process_fcn(MONITOR_EXIT_NORMAL);
    monitor_end_library_fcn();
    (*real_u_exit)(status);

    /* Never reached, but silence a compiler warning. */
    exit(status);
}

void
foilbase__Exit(int status)
{
    monitor_normal_init();
    MONITOR_DEBUG1("\n");

    monitor_end_process_fcn(MONITOR_EXIT_NORMAL);
    monitor_end_library_fcn();
    (*real_u_exit)(status);

    /* Never reached, but silence a compiler warning. */
    exit(status);
}

/*
 *  Library constructor (init) and destructor (fini) functions and
 *  __libc_start_main() are all dynamic only.
 */
void __attribute__ ((constructor))
monitor_library_init_constructor(void)
{
    MONITOR_DEBUG1("\n");
    monitor_begin_library_fcn();
}

void __attribute__ ((destructor))
monitor_library_fini_destructor(void)
{
    MONITOR_DEBUG1("\n");
    monitor_end_library_fcn();
}
