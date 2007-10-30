/*
 *  The non-threaded part of libmonitor.
 *
 *  $Id: monitor_main.c,v 1.12 2007/06/21 22:08:47 krentel Exp krentel $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
monitor_init_process(struct monitor_start_main_args *main_args)
{
    MONITOR_DEBUG("(default callback) args = %p\n", main_args);
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
monitor_init_thread(unsigned num)
{
    MONITOR_DEBUG("(default callback) num = %u\n", num);
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
extern main_fcn_t main;
extern exit_fcn_t hide_u_exit;
#endif

static start_main_fcn_t  *real_start_main = NULL;
static main_fcn_t        *real_main = NULL;
static exit_fcn_t        *real_u_exit = NULL;
static struct monitor_start_main_args main_args;

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

#ifdef MONITOR_STATIC
    real_main = &main;
    real_u_exit = &hide_u_exit;
#else
    MONITOR_REQUIRE_DLSYM(real_start_main, "__libc_start_main");
    MONITOR_REQUIRE_DLSYM(real_u_exit, "_exit");
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
monitor_begin_process_fcn(struct monitor_start_main_args *main_args)
{
    MONITOR_RUN_ONCE(begin_process);

    MONITOR_DEBUG("\n");
    monitor_normal_init();

    MONITOR_DEBUG("calling monitor_init_process() ...\n");
    monitor_init_process(main_args);
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
 *  Static case -- we get here directly, by linking with a modified
 *  crt1.o that renames "main" to "monitor_faux_main".  In this case,
 *  there are no library init/fini functions, and we have to fake the
 *  start main args.
 *
 *  Dynamic case -- we get into __libc_start_main via LD_PRELOAD.
 */
int
monitor_faux_main(int argc, char **argv, char **env)
{
    int ret;

    MONITOR_DEBUG("\n");

#ifdef MONITOR_STATIC
    main_args.program_name = "program name";
    main_args.main = &main;
    main_args.argc = argc;
    main_args.argv = argv;
    main_args.init = NULL;
    main_args.fini = NULL;
    main_args.rtld_fini = NULL;
    main_args.stack_end = NULL;
    real_main = &main;
    monitor_begin_library_fcn();
#endif

    monitor_begin_process_fcn(&main_args);

    if (atexit(&monitor_end_process_fcn) != 0) {
	MONITOR_ERROR("atexit failed\n");
    }

    asm(".globl monitor_unwind_fence1");
    asm("monitor_unwind_fence1:");
    ret = (*real_main)(argc, argv, env);
    asm(".globl monitor_unwind_fence2");
    asm("monitor_unwind_fence2:");

    monitor_end_process_fcn();

#ifdef MONITOR_STATIC
    monitor_end_library_fcn();
#endif

    return (ret);
}

/*
 *  _exit and _Exit bypass library fini functions, so we need to call
 *  them here.
 */
void
_exit(int status)
{
    MONITOR_DEBUG("\n");

    monitor_end_process_fcn();
    monitor_end_library_fcn();

    (*real_u_exit)(status);

    /* Never reached, but silence a compiler warning. */
    exit(status);
}

void
_Exit(int status)
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
static int
monitor_start_main ( START_MAIN_PARAM_LIST )
{
    MONITOR_DEBUG("\n");

    main_args.program_name = "program name";
    main_args.main = main;
    main_args.argc = argc;
    main_args.argv = argv;
    main_args.init = init;
    main_args.fini = fini;
    main_args.rtld_fini = rtld_fini;
    main_args.stack_end = stack_end;
    real_main = main;

    (*real_start_main)(monitor_faux_main, argc, argv, init, fini,
		       rtld_fini, stack_end);

    /* Never reached. */
    return (0);
}

int
__libc_start_main ( START_MAIN_PARAM_LIST )
{
    MONITOR_DEBUG("\n");
    return monitor_start_main ( START_MAIN_ARG_LIST );
}

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
