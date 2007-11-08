/*
 *  The non-threaded part of libmonitor.
 *
 *  $Id$
 */

#include "config.h"
#include <sys/types.h>
#ifdef MONITOR_DYNAMIC
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 0
#include <mcheck.h>
#endif

#include "common.h"
#include "monitor.h"

#ifdef MONITOR_DEBUG_DEFAULT_ON
int monitor_debug = 1;
#else
int monitor_debug = 0;
#endif

/*
 *----------------------------------------------------------------------
 *  MONITOR GLOBAL DECLARATIONS
 *----------------------------------------------------------------------
 */

#define START_MAIN_PARAM_LIST 			\
    int (*main) (int, char **, char **),	\
    int argc,					\
    char **argv,				\
    void (*init) (void),			\
    void (*fini) (void),			\
    void (*rtld_fini) (void),			\
    void *stack_end

#define START_MAIN_ARG_LIST  \
    main, argc, argv, init, fini, rtld_fini, stack_end

typedef int start_main_fcn_t ( START_MAIN_PARAM_LIST );
typedef int main_fcn_t(int, char **, char **);
typedef void exit_fcn_t(int);

typedef void free_fcn_t(void *);
typedef void *malloc_fcn_t(size_t);

#ifdef MONITOR_STATIC
extern main_fcn_t __real_main;
extern exit_fcn_t __real__exit;
#endif

static start_main_fcn_t  *real_start_main = NULL;
static main_fcn_t  *real_main = NULL;
static exit_fcn_t  *real_u_exit = NULL;

static free_fcn_t   *real_free   = NULL;
static malloc_fcn_t *real_malloc = NULL;

static int monitor_argc = 0;
static char **monitor_argv = NULL;
static char **monitor_envp = NULL;

volatile static char monitor_init_library_called = 0;
volatile static char monitor_fini_library_called = 0;
volatile static char monitor_init_process_called = 0;
volatile static char monitor_fini_process_called = 0;

extern void *monitor_unwind_fence1;
extern void *monitor_unwind_fence2;

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
void
monitor_early_init(void)
{
    char *options;

    MONITOR_RUN_ONCE(early_init);

#if 0
    mtrace();
#endif
    if (! monitor_debug) {
	if (getenv("MONITOR_DEBUG") != NULL)
	    monitor_debug = 1;
    }

    MONITOR_DEBUG("debug = %d\n", monitor_debug);
}

/*
 *  Run at library init time (dynamic), or in monitor faux main
 *  (static).
 */
static void
monitor_normal_init(void)
{
    extern void monitor_debug_pthread(void);
    MONITOR_RUN_ONCE(normal_init);

    monitor_debug_pthread();
    monitor_early_init();
    MONITOR_DEBUG1("\n");

    /*
     * Always get _exit() first so that we have a way to exit if
     * needed.
     */
    MONITOR_GET_REAL_NAME_WRAP(real_u_exit, _exit);
#ifdef MONITOR_STATIC
    real_main = &__real_main;
#else
    MONITOR_REQUIRE_DLSYM(real_start_main, "__libc_start_main");
#endif

    MONITOR_DEBUG1("Trying to set up free ...\n");
    MONITOR_GET_REAL_NAME(real_free,free);

    MONITOR_DEBUG1("Trying to set up malloc ...\n");
    MONITOR_GET_REAL_NAME(real_malloc,malloc);
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

void
monitor_begin_process_fcn(void)
{
    monitor_normal_init();

    MONITOR_DEBUG1("calling monitor_init_process() ...\n");
    monitor_init_process(monitor_argv[0], &monitor_argc,
			 monitor_argv, (unsigned)getpid());
    monitor_init_process_called = 1;
    monitor_fini_process_called = 0;
    monitor_fini_library_called = 0;

#ifdef MONITOR_USE_PTHREADS
    monitor_thread_release();
#endif
}

void
monitor_end_process_fcn(void)
{
  MONITOR_DEBUG1("Enter monitor_end_process after main return ...\n");
    if (monitor_fini_process_called)
	return;

#ifdef MONITOR_USE_PTHREADS
    monitor_thread_shootdown();
#endif

    MONITOR_DEBUG1("end_process after main return calling monitor_fini_process() ...\n");
    monitor_fini_process();
    monitor_fini_process_called = 1;
}

void
monitor_atexit_end_process_fcn(void)
{
  MONITOR_DEBUG1("atexit handler entered ...\n");
    if (monitor_fini_process_called)
	return;

#ifdef MONITOR_USE_PTHREADS
    monitor_thread_shootdown();
#endif

    MONITOR_DEBUG1("atexit handler calling monitor_fini_process() ...\n");
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
    int ret;

    MONITOR_DEBUG1("\n");
    monitor_argc = argc;
    monitor_argv = argv;
    monitor_envp = envp;
#ifdef MONITOR_STATIC
    real_main = &__real_main;
#endif

    asm(".globl monitor_unwind_fence1");
    asm("monitor_unwind_fence1:");
    monitor_begin_process_fcn();

    if (atexit(&monitor_atexit_end_process_fcn) != 0) {
	MONITOR_ERROR1("atexit failed\n");
    }
    ret = (*real_main)(monitor_argc, monitor_argv, monitor_envp);

    monitor_end_process_fcn();
    asm(".globl monitor_unwind_fence2");
    asm("monitor_unwind_fence2:");

    return (ret);
}

/*
 *  Returns: 1 if address is at the bottom of the application's call
 *  stack, else 0.
 */
int
monitor_unwind_process_bottom_frame(void *addr)
{
  void **ip = (void **) addr;
  int ret;
#if 0
  MONITOR_DEBUG("frame: addr=%p\n",ip);
  MONITOR_DEBUG("frame fence1: %p, fence2 = %p\n",&monitor_unwind_fence1,&monitor_unwind_fence2);

  MONITOR_DEBUG("frame comparison 1 = %d\n",&monitor_unwind_fence1 <= ip);
  MONITOR_DEBUG("frame comparison 2 = %d\n",ip <= &monitor_unwind_fence2);
#endif
    ret = (&monitor_unwind_fence1 <= ip) && (ip <= &monitor_unwind_fence2);
#if 0
    MONITOR_DEBUG("frame:ret = %d\n",ret);
#endif
    return ret;
}

#ifdef MONITOR_DYNAMIC
int
__libc_start_main ( START_MAIN_PARAM_LIST )
{
    MONITOR_DEBUG1("\n");
    real_main = main;

    (*real_start_main)(__wrap_main, argc, argv, init, fini,
		       rtld_fini, stack_end);

    /* Never reached. */
    return (0);
}
#endif  /* MONITOR_DYNAMIC */

/*
 *  _exit and _Exit bypass library fini functions, so we need to call
 *  them here (in the dynamic case).
 */
void
MONITOR_WRAP_NAME(_exit) (int status)
{
    MONITOR_DEBUG1("\n");

    monitor_end_process_fcn();
#ifdef MONITOR_DYNAMIC
    monitor_end_library_fcn();
#endif

    (*real_u_exit)(status);

    /* Never reached, but silence a compiler warning. */
    exit(status);
}

void
MONITOR_WRAP_NAME(_Exit) (int status)
{
    MONITOR_DEBUG1("\n");

    monitor_end_process_fcn();
#ifdef MONITOR_DYNAMIC
    monitor_end_library_fcn();
#endif

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
    MONITOR_DEBUG1("\n");
    monitor_begin_library_fcn();
}

void __attribute__ ((destructor))
monitor_library_fini_destructor(void)
{
    MONITOR_DEBUG1("\n");
    monitor_end_library_fcn();
}

#endif  /* MONITOR_DYNAMIC */

#ifdef MALLOC_DEBUG
/*
 *** Override free to see what is up
 */

void free(void *p){
  MONITOR_DEBUG("free called with p = %p\n",p);
  real_free(p);
}

void *malloc(size_t n){
  void *ret = real_malloc(n);
  MONITOR_DEBUG("malloc ret = %p\n",ret);
  return ret;
}
#endif

