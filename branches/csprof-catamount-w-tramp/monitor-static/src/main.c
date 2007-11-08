/*
 *  The non-threaded part of libmonitor.
 *
 *  $Id$
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef MONITOR_DYNAMIC
#include <dlfcn.h>
#endif

#include "monitor.h"
#include "monitor_decls.h"

#ifdef MONITOR_DEBUG_ON
int monitor_opt_debug = 1;
#else
int monitor_opt_debug = 0;
#endif

/*
 *----------------------------------------------------------------------
 *  CALLBACK FUNCTIONS
 *
 *  Define the defaults here as weak symbols and allow the user to
 *  override a subset of these.
 *----------------------------------------------------------------------
 */

void __attribute__ ((weak))
monitor_init_library(void)
{
    MONITOR_DEBUG("(default callback)\n");
}

void __attribute__ ((weak))
monitor_fini_library(void)
{
    MONITOR_DEBUG("(default callback)\n");
}

void __attribute__ ((weak))
monitor_init_process(char *process, int *argc, char **argv, unsigned pid)
{
    int i;

    MONITOR_DEBUG("(default callback) process = %s, argc = %d, pid = %u\n",
		  process, *argc, pid);
    for (i = 0; i < *argc; i++) {
	MONITOR_DEBUG("argv[%d] = %s\n", i, argv[i]);
    }
}

void __attribute__ ((weak))
monitor_fini_process(void)
{
    MONITOR_DEBUG("(default callback)\n");
}

void __attribute__ ((weak))
monitor_init_thread_support(void)
{
    MONITOR_DEBUG("(default callback)\n");
}

void * __attribute__ ((weak))
monitor_init_thread(unsigned tid)
{
    MONITOR_DEBUG("(default callback) tid = %u\n", tid);
    return (NULL);
}

void __attribute__ ((weak))
monitor_fini_thread(void *user_data)
{
    MONITOR_DEBUG("(default callback) data = %p\n", user_data);
}

void __attribute__ ((weak))
monitor_dlopen(const char *library)
{
    MONITOR_DEBUG("(default callback) library = %s\n", library);
}

/*
 *----------------------------------------------------------------------
 *  MONITOR GLOBAL VARIABLES
 *----------------------------------------------------------------------
 */

static char monitor_init_library_called = 0;
static char monitor_fini_library_called = 0;
static char monitor_init_process_called = 0;
static char monitor_fini_process_called = 0;

#ifdef MONITOR_STATIC
extern main_fcn_t __real_main;
extern exit_fcn_t __real__exit;
#endif

static start_main_fcn_t  *real_start_main = NULL;
static main_fcn_t        *real_main = NULL;
static exit_fcn_t        *real_u_exit = NULL;

/*
 *----------------------------------------------------------------------
 *  MONITOR INIT FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Normally run as part of monitor_normal_init(), but may be run
 *  earlier if someone calls pthread_create() before our library init
 *  function (dynamic case).
 */
static void
monitor_early_init(void)
{
    char *options;

    MONITOR_RUN_ONCE(early_init);

    if (! monitor_opt_debug) {
	if (getenv("MONITOR_DEBUG") != NULL)
	    monitor_opt_debug = 1;
    }

    MONITOR_DEBUG("debug = %d\n", monitor_opt_debug);
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
    MONITOR_DEBUG("\n");

    /*
     * Always get _exit() first so that we have a way to exit if
     * needed.
     */
    MONITOR_GET_REAL_NAME(real_u_exit, _exit);
#ifdef MONITOR_STATIC
    real_main = &__real_main;
#else
    MONITOR_REQUIRE_DLSYM(real_start_main, "__libc_start_main");
#endif
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

    MONITOR_DEBUG("\n");
    monitor_normal_init();

    MONITOR_DEBUG("calling monitor_init_library() ...\n");
    monitor_init_library();
    monitor_init_library_called = 1;
}

static void
monitor_end_library_fcn(void)
{
    MONITOR_RUN_ONCE(end_library);

    MONITOR_DEBUG("calling monitor_fini_library() ...\n");
    monitor_fini_library();
    monitor_fini_library_called = 1;
}

static void
monitor_begin_process_fcn(struct monitor_main_args *main_args)
{
    MONITOR_RUN_ONCE(begin_process);

    MONITOR_DEBUG("\n");
    monitor_normal_init();

    MONITOR_DEBUG("calling monitor_init_process() ...\n");
    monitor_init_process(main_args->argv[0], &main_args->argc,
			 main_args->argv, (unsigned)getpid());
    monitor_init_process_called = 1;
}

static void
monitor_end_process_fcn(void)
{
    MONITOR_RUN_ONCE(end_process);

    MONITOR_DEBUG("calling monitor_fini_process() ...\n");
    monitor_fini_process();
    monitor_fini_process_called = 1;
}

/*
 *----------------------------------------------------------------------
 *  EXTERNAL OVERRIDES and their helper functions
 *----------------------------------------------------------------------
 */

/*
 *  Static case -- we get into __wrap_main() directly, by editing the
 *  link line so that the system call to main() comes here instead.
 *  In this case, there are no library init/fini functions.
 *
 *  Dynamic case -- we get into __libc_start_main() via LD_PRELOAD.
 */
int
__wrap_main(int argc, char **argv, char **envp)
{
    struct monitor_main_args main_args;
    int ret;

    MONITOR_DEBUG("\n");
    main_args.argc = argc;
    main_args.argv = argv;
    main_args.envp = envp;

#ifdef MONITOR_STATIC
    real_main = &__real_main;
    monitor_begin_library_fcn();
#endif

    monitor_begin_process_fcn(&main_args);

    if (atexit(&monitor_end_process_fcn) != 0) {
	MONITOR_ERROR("atexit failed\n");
    }

    asm(".globl monitor_unwind_fence1");
    asm("monitor_unwind_fence1:");
    ret = (*real_main)(argc, argv, envp);
    asm(".globl monitor_unwind_fence2");
    asm("monitor_unwind_fence2:");

    monitor_end_process_fcn();
#ifdef MONITOR_STATIC
    monitor_end_library_fcn();
#endif

    return (ret);
}

#ifdef MONITOR_DYNAMIC
int
__libc_start_main ( START_MAIN_PARAM_LIST )
{
    MONITOR_DEBUG("\n");
    real_main = main;

    (*real_start_main)(__wrap_main, argc, argv, init, fini,
		       rtld_fini, stack_end);

    /* Never reached. */
    return (0);
}
#endif

/*
 *  _exit and _Exit bypass library fini functions, so we need to call
 *  them here.
 */
void
MONITOR_WRAP_NAME(_exit) (int status)
{
    MONITOR_DEBUG("\n");

    monitor_end_process_fcn();
    monitor_end_library_fcn();

    (*real_u_exit)(status);

    /* Never reached, but silence a compiler warning. */
    exit(status);
}

void
MONITOR_WRAP_NAME(_Exit) (int status)
{
    MONITOR_DEBUG("\n");

    monitor_end_process_fcn();
    monitor_end_library_fcn();

    (*real_u_exit)(status);

    /* Never reached, but silence a compiler warning. */
    exit(status);
}

/*
 *  Library constructor (init) and destructor (fini) functions and
 *  __libc_start_main() are all dynamic only.
 */
#ifdef MONITOR_DYNAMIC
void __attribute__ ((constructor))
monitor_library_init_constructor(void)
{
    MONITOR_DEBUG("\n");
    monitor_begin_library_fcn();
}

void __attribute__ ((destructor))
monitor_library_fini_destructor(void)
{
    MONITOR_DEBUG("\n");
    monitor_end_library_fcn();
}

#endif  /* MONITOR_DYNAMIC */
