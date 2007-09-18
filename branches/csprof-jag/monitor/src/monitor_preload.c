/* $Id: monitor_preload.c,v 1.15 2007/05/29 22:23:39 krentel Exp krentel $ */

/* This code has been stripped out of HPCToolkit and heavily modified by Philip Mucci,
   University of Tennessee and SiCortex. This is OPEN SOURCE. */

/* -*-Mode: C;-*- */

/****************************************************************************
//
// File: 
//    $Source: /home/krentel/monitor/src/RCS/monitor_preload.c,v $
//
// Purpose:
//    Implements monitoring that is launched with a preloaded library,
//    something that only works for dynamically linked applications.
//    The library intercepts the beginning execution point of the
//    process and then starts monitoring. When the process exits,
//    control will be transferred back to this library where profiling
//    is finalized.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

/**************************** User Include Files ****************************/

#include <sys/queue.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

#include "monitor_preload.h"
#include "monitor.h"

#define MONITOR_DEBUG(...)  do {				\
    if (monitor_opt_debug) {					\
	fprintf(stderr, "monitor debug>> %s: ", __func__);	\
	fprintf(stderr, __VA_ARGS__ );				\
    }								\
} while (0)

#define MONITOR_WARN(...)  do {					\
    fprintf(stderr, "monitor warning>> %s: ", __func__);	\
    fprintf(stderr, __VA_ARGS__ );				\
} while (0)

#define MONITOR_ERROR(...)  do {				\
    fprintf(stderr, "monitor error>> %s: ", __func__);		\
    fprintf(stderr, __VA_ARGS__ );				\
} while (0)

#ifdef __GNUC__

static void _monitor_init_library(void) {
    MONITOR_DEBUG("(default callback)\n");
}
static void _monitor_fini_library(void) {
    MONITOR_DEBUG("(default callback)\n");
}
static void _monitor_init_process(struct monitor_start_main_args *main_args) {
    MONITOR_DEBUG("(%p) (default callback)\n", main_args);
}
static void _monitor_fini_process(void) {
    MONITOR_DEBUG("(default callback)\n");
}
static void _monitor_init_thread_support(void) {
    MONITOR_DEBUG("(default callback)\n");
}
static void * _monitor_init_thread(unsigned num) {
    MONITOR_DEBUG("(%u) (default callback)\n", num);
    return (NULL);
}
static void _monitor_fini_thread(void *init_thread_data) {
    MONITOR_DEBUG("(%p) (default callback)\n", init_thread_data);
}
static void _monitor_dlopen(const char *library) {
    MONITOR_DEBUG("(%p) (default callback)\n", library);
}

void monitor_init_library(void) __attribute__ ((weak, alias ("_monitor_init_library")));
void monitor_fini_library(void) __attribute__ ((weak, alias ("_monitor_fini_library")));
void monitor_init_process(struct monitor_start_main_args *main_args)
    __attribute__ ((weak, alias ("_monitor_init_process")));
void monitor_fini_process(void) __attribute__ ((weak, alias ("_monitor_fini_process")));
void monitor_init_thread_support(void)
    __attribute__ ((weak, alias ("_monitor_init_thread_support")));
void *monitor_init_thread(unsigned num) __attribute__ ((weak, alias ("_monitor_init_thread")));
void monitor_fini_thread(void *init_thread_data)
    __attribute__ ((weak, alias ("_monitor_fini_thread")));
void monitor_dlopen(const char *library) __attribute__ ((weak, alias ("_monitor_dlopen")));

#endif

#define MONITOR_FINI_PROCESS  monitor_fini_process_function()

#define MONITOR_FINI_LIBRARY  do {					\
    if (! monitor_library_fini_done) {					\
	MONITOR_DEBUG("calling monitor_fini_library() ...\n");		\
	monitor_fini_library();						\
	monitor_library_fini_done = 1;					\
    }									\
} while (0)

/**************************** Forward Declarations **************************/

static int  monitor_libc_start_main PARAMS_START_MAIN;
static void monitor_libc_start_main_fini(); 
static int  monitor_execv  PARAMS_EXECV;
static int  monitor_execvp PARAMS_EXECVP;
static int  monitor_execve PARAMS_EXECVE;
static pid_t monitor_fork();
static void monitor_init_sighandlers();
static int monitor_pthread_create PARAMS_PTHREAD_CREATE;
static void* monitor_pthread_create_start_routine(void* arg);
static void monitor_pthread_cleanup_routine(void* arg);

/*************************** Variable Declarations **************************/

/* Intercepted libc and libpthread routines: set when the library is initialized */

static libc_start_main_fptr_t      real_start_main = NULL;
static libc_start_main_fini_fptr_t real_start_main_fini = NULL;
static execv_fptr_t                real_execv = NULL;
static execvp_fptr_t               real_execvp = NULL;
static execve_fptr_t               real_execve = NULL;
static fork_fptr_t                 real_fork = NULL;
static _exit_fptr_t                real__exit = NULL;
static exit_fptr_t                 real_exit = NULL;
static pthread_create_fptr_t       real_pthread_create = NULL;
#if defined(USE_PTHREAD_SELF)
static pthread_self_fptr_t         real_pthread_self = NULL;
#endif
static dlopen_fptr_t               real_dlopen = NULL;

typedef int (*pthread_mutex_lock_fptr_t)(pthread_mutex_t *mutex);
typedef int (*pthread_kill_fptr_t)(pthread_t thread, int signum);

static pthread_mutex_lock_fptr_t	real_pthread_mutex_lock;
static pthread_mutex_lock_fptr_t	real_pthread_mutex_unlock;
static pthread_kill_fptr_t		real_pthread_kill;
static pthread_self_fptr_t		real_pthread_self;

/* Information about the process */

static int monitor_argc = 0;
static char **monitor_argv = NULL;
static int monitor_root_pid;
static char *monitor_cmd = NULL;

/* Library options */

int monitor_opt_debug = 0;
int monitor_opt_error = 0;

/* Internal state

   For the following, we hope/assume assignment is
   atomic but increment/decrement is not. Some day, 
   these will get a lock or true atomic operations */

static volatile int monitor_library_fini_done = 0; 		/* Assigned */

/*------------------------------------------------------------------------
 *
 * Keep a list of running threads so we can invoke their fini thread
 * functions in the case that one thread calls exit() before the
 * others call pthread_exit().
 *
 *------------------------------------------------------------------------
 */
#define MONITOR_EXIT_CLEANUP_SIGNAL  SIGUSR2
#define MONITOR_POLL_USLEEP_TIME  200000
#define MONITOR_TN_MAGIC  0x6d746e00

/* The global thread mutex protects the monitor's list of threads and
 * related state variables.
 *
 * Note: After monitor begins exit cleanup (some thread calls exit or
 * exec), then we don't insert or delete any new thread nodes into the
 * list.  Instead, we set the tn_* variables to indicate the thread
 * status and when it is finished.
 */
static pthread_mutex_t
monitor_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

static int  monitor_thread_num = 0;
static char monitor_init_process_done = 0;
static char monitor_thread_support_done = 0;
static char monitor_thread_support_deferred = 0;

static char monitor_early_init_done = 0;
static char monitor_normal_init_done = 0;
static char monitor_thread_init_done = 0;
static char monitor_in_exit_cleanup = 0;
static char monitor_exit_cleanup_done = 0;

static __thread struct monitor_thread_node *monitor_my_tnode;

/* Some Linuxes don't define LIST_FIRST and LIST_NEXT !?
 * How brain-damaged is that !?
 */
#define LIST_FIRST(head)         ((head)->lh_first)
#define LIST_NEXT(elm, field)    ((elm)->field.le_next)

typedef void *(pthread_func_t)(void *);

struct monitor_thread_node {
    LIST_ENTRY(monitor_thread_node) tn_links;
    int    tn_magic;
    int    tn_thread_num;
    pthread_t  tn_self;
    pthread_func_t *tn_start_routine;
    void  *tn_arg;
    void  *tn_user_data;
    char   tn_appl_started;
    char   tn_fini_started;
    char   tn_fini_done;
};

static LIST_HEAD(, monitor_thread_node) monitor_thread_list;

#define MONITOR_THREAD_LOCK    (*real_pthread_mutex_lock)(&monitor_thread_mutex)
#define MONITOR_THREAD_UNLOCK  (*real_pthread_mutex_unlock)(&monitor_thread_mutex)

static void
monitor_exit_signal_handler(int signum)
{
    struct monitor_thread_node *tn = monitor_my_tnode;

    if (tn == NULL || tn->tn_magic != MONITOR_TN_MAGIC) {
	MONITOR_WARN("missing thread specific data -- "
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

static struct monitor_thread_node *
monitor_make_thread_node(void)
{
    struct monitor_thread_node *tn;

    tn = malloc(sizeof(struct monitor_thread_node));
    if (tn == NULL) {
	MONITOR_ERROR("unable to malloc thread node\n");
    }
    memset(tn, 0, sizeof(struct monitor_thread_node));
    tn->tn_magic = MONITOR_TN_MAGIC;

    return (tn);
}

/* This runs in the new thread.
 * 
 * Returns: 0 on success, or 1 if at exit cleanup and thus we don't
 * allow any new threads.
 */ 
static int
monitor_link_thread_node(struct monitor_thread_node *tn)
{
    MONITOR_THREAD_LOCK;
    if (monitor_in_exit_cleanup) {
	MONITOR_THREAD_UNLOCK;
	return (1);
    }

    tn->tn_thread_num = ++monitor_thread_num;
    tn->tn_self= (*real_pthread_self)();
    monitor_my_tnode = tn;
    LIST_INSERT_HEAD(&monitor_thread_list, tn, tn_links);

    MONITOR_THREAD_UNLOCK;
    return (0);
}

static void
monitor_unlink_thread_node(struct monitor_thread_node *tn)
{
    MONITOR_THREAD_LOCK;

    /* Don't delete the thread node if in exit cleanup, just mark the
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
monitor_cleanup_leftover_threads(void)
{
    struct monitor_thread_node *tn, *my_tn;
    struct sigaction my_action;
    sigset_t empty_set;
    pthread_t self;
    int num_unfinished;

    MONITOR_DEBUG("\n");

    /*
     * Install the signal handler for MONITOR_EXIT_CLEANUP_SIGNAL.
     * Note: the signal handler is process-wide.
     */
    sigemptyset(&empty_set);
    my_action.sa_handler = monitor_exit_signal_handler;
    my_action.sa_mask = empty_set;
    my_action.sa_flags = 0;
    if (sigaction(MONITOR_EXIT_CLEANUP_SIGNAL, &my_action, NULL) != 0) {
	MONITOR_ERROR("sigaction failed\n");
    }

    /*
     * Walk through the list of unfinished threads, send a signal to
     * force them into their fini_thread functions, and wait until
     * they all finish.  But don't signal ourself.
     *
     * Note: we may want to add a timeout here.
     */
    self = (*real_pthread_self)();
    my_tn = NULL;
    for (;;) {
	num_unfinished = 0;
	for (tn = LIST_FIRST(&monitor_thread_list);
	     tn != NULL;
	     tn = LIST_NEXT(tn, tn_links)) {

	     if (pthread_equal(self, tn->tn_self)) {
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

    /* See if we need to run fini_thread from this thread.
     */
    if (my_tn != NULL && !my_tn->tn_fini_started) {
	MONITOR_DEBUG("calling monitor_fini_thread(data = %p, num = %d) ...\n",
		      tn->tn_user_data, tn->tn_thread_num);
	tn->tn_fini_started = 1;
	monitor_fini_thread(tn->tn_user_data);
	tn->tn_fini_done = 1;
    }
}

static void
monitor_fini_process_function(void)
{
    int first;

    MONITOR_DEBUG("\n");

    /* Avoid using pthread functions (mutex) if the application has
     * never called pthread_create().
     */
    if (monitor_thread_init_done) {
	MONITOR_THREAD_LOCK;
	first = !monitor_in_exit_cleanup;
	monitor_in_exit_cleanup = 1;
	MONITOR_THREAD_UNLOCK;
    } else {
	first = !monitor_in_exit_cleanup;
	monitor_in_exit_cleanup = 1;
    }

    if (first && monitor_thread_init_done)
	monitor_cleanup_leftover_threads();

    /* Don't let any thread advance until all fini_thread and
     * fini_process functions have returned.
     */
    if (first) {
	MONITOR_DEBUG("calling monitor_fini_process() ...\n");
	monitor_fini_process();
	monitor_exit_cleanup_done = 1;
    } else {
	while (! monitor_exit_cleanup_done)
	    usleep(MONITOR_POLL_USLEEP_TIME);
    }
}

/* Force the running of fini_thread for any outstanding threads, and
 * then run fini_process.
 */
void
monitor_force_fini_process(void)
{
    monitor_fini_process_function();
}

/****************************************************************************
 * Library initialization and finalization
 ****************************************************************************/

/* Var and name can't be expressions here.
 */
#define MONITOR_TRY_SYMBOL(var, name)  do {		\
    if (var == NULL) {					\
	const char *err_str;				\
	dlerror();					\
	var = dlsym(RTLD_NEXT, (name));			\
	err_str = dlerror();				\
	if (err_str != NULL) {				\
	    MONITOR_WARN("dlsym(%s) failed: %s\n",	\
			 (name), err_str);		\
	}						\
	MONITOR_DEBUG("%s() = %p\n", (name), var);	\
    }							\
} while (0)

#define MONITOR_REQUIRE_SYMBOL(var, name)  do {		\
    if (var == NULL) {					\
	const char *err_str;				\
	dlerror();					\
	var = dlsym(RTLD_NEXT, (name));			\
	err_str = dlerror();				\
	if (var == NULL) {				\
	    MONITOR_ERROR("dlsym(%s) failed: %s\n",	\
			 (name), err_str);		\
	}						\
	MONITOR_DEBUG("%s() = %p\n", (name), var);	\
    }							\
} while (0)

/*
 *  Normally run as part of monitor_normal_init(), but may be run
 *  earlier if someone calls pthread_create() before our library init
 *  function.
 */
static void
monitor_early_init(void)
{
    char *options;

    if (monitor_early_init_done)
	return;

    monitor_opt_error = 0;
    monitor_opt_debug = 0;

    options = getenv("MONITOR_OPTIONS");
    if (options != NULL && strstr(options, "SIGABRT"))
	monitor_opt_error |= MONITOR_SIGABRT;
    if (options != NULL && strstr(options, "SIGINT"))
	monitor_opt_error |= MONITOR_SIGINT;
    if (options != NULL && strstr(options, "NONZERO_EXIT"))
	monitor_opt_error |= MONITOR_NONZERO_EXIT;
    if ((options != NULL && strstr(options, "DEBUG") != NULL)
	|| getenv("MONITOR_DEBUG") != NULL)
	monitor_opt_debug = 1;

    MONITOR_DEBUG("debug = %d, error= %d\n",
		  monitor_opt_debug, monitor_opt_error);

    monitor_early_init_done = 1;
}

/*
 *  Run on the first call to pthread_create().
 */
static void
monitor_thread_init(void)
{
    if (monitor_thread_init_done)
	return;

    LIST_INIT(&monitor_thread_list);
    MONITOR_REQUIRE_SYMBOL(real_pthread_create, "pthread_create");
    MONITOR_REQUIRE_SYMBOL(real_pthread_kill,   "pthread_kill");
    MONITOR_REQUIRE_SYMBOL(real_pthread_mutex_lock,   "pthread_mutex_lock");
    MONITOR_REQUIRE_SYMBOL(real_pthread_mutex_unlock, "pthread_mutex_unlock");
    MONITOR_REQUIRE_SYMBOL(real_pthread_self,  "pthread_self");

    monitor_thread_init_done = 1;
}

/*
 *  Run at library init time.
 */
static void
monitor_normal_init(void)
{
    monitor_early_init();
    if (monitor_normal_init_done)
	return;

    MONITOR_TRY_SYMBOL(real_start_main, "__libc_start_main");
    MONITOR_REQUIRE_SYMBOL(real_start_main, "__BP___libc_start_main");
    MONITOR_TRY_SYMBOL(real_dlopen, "dlopen");
    MONITOR_TRY_SYMBOL(real_execv,  "execv");
    MONITOR_TRY_SYMBOL(real_execvp, "execvp");
    MONITOR_TRY_SYMBOL(real_execve, "execve");
    MONITOR_TRY_SYMBOL(real_fork,   "fork");
    MONITOR_TRY_SYMBOL(real_exit,   "exit");
    MONITOR_TRY_SYMBOL(real__exit,  "_exit");
    MONITOR_TRY_SYMBOL(real_pthread_create, "pthread_create");

    monitor_normal_init_done = 1;

    MONITOR_DEBUG("calling monitor_init_library() ...\n");
    monitor_init_library();
}

/*
 *  Library constructor/deconstructor
 */
#ifdef __GNUC__
extern void __attribute__ ((constructor))
_monitor_init(void)
#else
extern void
_init(void)
#endif
{
    MONITOR_DEBUG("\n");
    monitor_normal_init();
}

#ifdef __GNUC__
extern void
_monitor_fini() __attribute__((destructor));
extern void
_monitor_fini()
#else
extern void _fini()
#endif
{
    MONITOR_DEBUG("\n");
    MONITOR_FINI_LIBRARY;
}

static int 
monitor_libc_start_main PARAMS_START_MAIN
{
  struct monitor_start_main_args main_args;
  char tmp[PATH_MAX], tmp2[PATH_MAX];

  MONITOR_DEBUG("\n");

  /* squirrel away for later use */
  monitor_argc = argc;
  monitor_argv = ubp_av;

  memset(tmp,0x0,sizeof(tmp));
  sprintf(tmp,"/proc/%d/exe",getpid());
  memset(tmp2,0x0,sizeof(tmp2));
  if (readlink(tmp,tmp2,PATH_MAX) == -1)
    {
      MONITOR_WARN("readlink(%s,%p,%d) failed! (%s)\n",tmp,tmp2,PATH_MAX,strerror(errno)); 
      monitor_cmd = strdup("Unknown");
    }
  else 
    monitor_cmd = strdup(tmp2);

  /* This line doesn't work on PPC Linux */
  /* monitor_cmd = ubp_av[0];  */
  real_start_main_fini = fini;

  /* Keep track of base pid */
  monitor_root_pid = getpid();

  /* initialize signal handlers */
  monitor_init_sighandlers();
  
  if (atexit(monitor_force_fini_process) == -1)
    {
      MONITOR_ERROR("atexit(%p) failed! (%s)\n",
		    monitor_force_fini_process, strerror(errno));
      monitor_real_exit(1);
    }

  /* EXTERNAL HOOK */
  MONITOR_DEBUG("calling monitor_init_process(args = %p) ...\n", &main_args);

  main_args.program_name = monitor_cmd;
  main_args.main = main;
  main_args.argc = monitor_argc;
  main_args.argv = monitor_argv;
  main_args.init = init;
  main_args.fini = monitor_libc_start_main_fini;
  main_args.rtld_fini = rtld_fini;
  main_args.stack_end = stack_end;

  monitor_init_process(&main_args);
  monitor_init_process_done = 1;

  /* If we've deferred thread_support because pthread_create was run
   * early, then do it now.  This also releases any waiting threads.
   */
  if (monitor_thread_support_deferred) {
      MONITOR_DEBUG("calling monitor_init_thread_support() deferred ...\n"); 
      monitor_init_thread_support();
      monitor_thread_support_done = 1;
  }

  /* launch the process */
  MONITOR_DEBUG("starting program: %s\n", monitor_cmd);

  (*real_start_main)(main_args.main, main_args.argc, main_args.argv, main_args.init,
		     main_args.fini, main_args.rtld_fini, main_args.stack_end);

  /* never reached */
  return (0);
}

static void 
monitor_libc_start_main_fini()
{
    MONITOR_DEBUG("\n");

    if (real_start_main_fini) {
	(*real_start_main_fini)();
    }
    MONITOR_FINI_PROCESS;

    if (monitor_cmd)
	free(monitor_cmd);
}

extern void 
monitor_parse_execl(const char*** argv, const char* const** envp,
		    const char* arg, va_list arglist)
{
  /* argv & envp are pointers to arrays of char* */
  /* va_start has already been called */

  const char* argp;
  int argvSz = 32, argc = 1;
  
  *argv = malloc((argvSz+1) * sizeof(const char*));
  if (!*argv) { MONITOR_ERROR("malloc() failed!\n"); }
  
  (*argv)[0] = arg;
  while ((argp = va_arg(arglist, const char*)) != NULL) { 
    if (argc > argvSz) {
      argvSz *= 2;
      *argv = realloc(*argv, (argvSz+1) * sizeof(const char*));
      if (!*argv) { MONITOR_ERROR("realloc() failed!\n"); }
    }
    (*argv)[argc] = argp;
    argc++;
  }
  (*argv)[argc] = NULL;
  
  if (envp != NULL) {
    *envp = va_arg(arglist, const char* const*);
  }

  { 
    int i;
    for (i = 0; i < argc; ++i) {
      MONITOR_DEBUG("arg %d: %s\n", i, (*argv)[i]);
    }
  }
  
  /* user calls va_end */
}

static int
monitor_execv PARAMS_EXECV
{
    MONITOR_DEBUG("execv(path = %s, ...)\n", path);

    /* Sanity check to defer turning off library as long as possible.
     */
    if (access(path, X_OK) != 0) {
	MONITOR_DEBUG("execv path not executable\n");
	return (-1);
    }

    MONITOR_FINI_PROCESS;
    MONITOR_FINI_LIBRARY;

    MONITOR_REQUIRE_SYMBOL(real_execv, "execv");
    return (*real_execv)(path, argv);
}

static int
monitor_execvp PARAMS_EXECVP
{
    MONITOR_DEBUG("execvp(file = %s, ...)\n", file);

    /* We better hope this succeeds because we will shut down the
     * library on this event.
     */
    MONITOR_FINI_PROCESS;
    MONITOR_FINI_LIBRARY;

    MONITOR_REQUIRE_SYMBOL(real_execvp, "execvp");

    return (*real_execvp)(file, argv);
}

static int
monitor_execve PARAMS_EXECVE
{
    MONITOR_DEBUG("execve(path = %s, ...)\n", path);

    /* Sanity check to defer turning off library as soon as possible.
     */
    if (access(path, X_OK) != 0) {
	MONITOR_DEBUG("execve path not executable\n");
	return (-1);
    }

    MONITOR_FINI_PROCESS;
    MONITOR_FINI_LIBRARY;

    return monitor_real_execve(path, argv, envp);
}

static pid_t 
monitor_fork()
{
  pid_t pid;

  MONITOR_DEBUG("\n"); 
  
  pid = monitor_real_fork();

  if (pid == 0) { 
    /* Initialize profiling for child process */

    /* Keep track of base pid */
    monitor_root_pid = getpid();

    /* Reset for subprocess, I don't think this is right.
    monitor_thread_support_called = 0;
    monitor_thread_count = 0; */

    /* EXTERNAL HOOK */
    MONITOR_DEBUG("fork -- monitor_init_process(NULL)\n");
    monitor_init_process(NULL);
    monitor_init_process_done = 1;
  } 
  /* Nothing to do for parent process */
  
  return pid;
}

static void __attribute__ ((noreturn))
monitor__exit(int status)
{
    MONITOR_DEBUG("_exit(%d)\n", status);

    MONITOR_REQUIRE_SYMBOL(real__exit, "_exit");

    (*real__exit)(status);
}

/****************************************************************************
 * Intercepted signals
 ****************************************************************************/

/* We allow the user to kill profiling by intercepting the certain
   signals.  This can be very useful on long-running or misbehaving
   applications. */

static void
monitor_sighandler(int sig)
{
  MONITOR_DEBUG("catching signal %d\n", sig); 
  
  signal(sig, SIG_DFL); /* return to default action */
    
  MONITOR_FINI_PROCESS;
  MONITOR_FINI_LIBRARY;
  
  if (sig == SIGABRT)
    abort();
  else if (sig == SIGINT)
    raise(SIGINT); /* This violates Unix standards...what do they think abort does? */
}

static void 
monitor_init_sighandler(int sig)
{
  typedef void (*sighandler_t)(int);
  sighandler_t dummy;

  dummy = signal(sig, SIG_IGN);
  
  if (dummy != SIG_IGN) 
    signal(sig, monitor_sighandler);
  else 
    {
      MONITOR_WARN("signal %d already has a handler.\n", sig);
      signal(sig, dummy);
    }
}

static void 
monitor_init_sighandlers()
{
  if (monitor_opt_error & MONITOR_SIGINT) 
    monitor_init_sighandler(SIGINT);   /* Ctrl-C */
  if (monitor_opt_error & MONITOR_SIGABRT)
    monitor_init_sighandler(SIGABRT);  /* abort() */
}

/* End stolen code */

static int
monitor_pthread_create PARAMS_PTHREAD_CREATE
{
    struct monitor_thread_node *tn;
    int first, ret;

    MONITOR_DEBUG("\n");

    /* It's possible to get here before _monitor_init().
     */
    monitor_early_init();

    /* There is no race condition to get here first because until now,
     * there is only one thread.
     */
    first = !monitor_thread_init_done;
    if (first)
	monitor_thread_init();

    /* Always launch the thread.  If we're in pthread_create too
     * early, then the new thread will spin-wait until init_process
     * and thread_support are done.
     */
    tn = monitor_make_thread_node();
    tn->tn_start_routine = start_routine;
    tn->tn_arg = arg;

    ret = (*real_pthread_create)
	(thread, attr, monitor_pthread_create_start_routine, (void *)tn);

    if (first) {
	/*
	 * Normally, we run thread_support here, on the first call to
	 * pthread_create.  But if we're here early, before
	 * libc_start_main, then defer thread_support until after
	 * init_process in libc_start_main.
	 */
	if (monitor_init_process_done) {
	    MONITOR_DEBUG("calling monitor_init_thread_support() ...\n");
	    monitor_init_thread_support();
	    monitor_thread_support_done = 1;
	} else {
	    MONITOR_DEBUG("deferring thread support\n");
	    monitor_thread_support_deferred = 1;
	}
    }

    return (ret);
}

/* This is called from real_pthread_create(), it's where the
 * newly-created thread begins.
 */
static void *
monitor_pthread_create_start_routine(void *arg)
{
    struct monitor_thread_node *tn = arg;
    void *rval;

    /* Wait for monitor_init_thread_support() to finish in the main
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

    MONITOR_DEBUG("(start of new thread)\n");

    /* Don't create any new threads after someone has called exit().
     */
    if (monitor_link_thread_node(tn) != 0) {
	MONITOR_WARN("trying to create new thread during exit cleanup -- "
		     "thread not started\n");
	return (NULL);
    }
    MONITOR_DEBUG("calling monitor_init_thread(num = %u) ...\n",
		  tn->tn_thread_num);
    tn->tn_user_data = monitor_init_thread(tn->tn_thread_num);

#if defined(USE_PTHREAD_CLEANUP_PUSH_POP)
    pthread_cleanup_push(monitor_pthread_cleanup_routine, tn);
#endif

    tn->tn_appl_started = 1;
    rval = (tn->tn_start_routine)(tn->tn_arg);

#if defined(USE_PTHREAD_CLEANUP_PUSH_POP)
    pthread_cleanup_pop(1);  
#else
    monitor_pthread_cleanup_routine(tn);
#endif

    return (rval);
}

/* We get here when the application thread has finished, either by
 * returning, pthread_exit() or being canceled.
 */
static void
monitor_pthread_cleanup_routine(void *arg)
{
    struct monitor_thread_node *tn = arg;

    MONITOR_DEBUG("(pthread cleanup)\n");

    if (tn == NULL)
	tn = monitor_my_tnode;
    if (tn == NULL || tn->tn_magic != MONITOR_TN_MAGIC) {
	MONITOR_WARN("missing thread-specific data\n");
	MONITOR_DEBUG("calling monitor_fini_thread(NULL)\n");
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

/****************************************************************************
 * Provided routines
 * Wrappers to be used by tool code
 ****************************************************************************/

void __attribute__ ((noreturn))
monitor_real_exit(int status)
{
    MONITOR_DEBUG("monitor_real_exit(%d)\n", status);

    MONITOR_TRY_SYMBOL(real_exit, "exit");
    if (real_exit == NULL) {
#if defined(SYS_exit)
        syscall(SYS_exit, status);
#elif defined(__NR_exit)
        syscall(__NR_exit, status);
#else
        MONITOR_ERROR("unable to find 'exit' or equivalent syscall\n");
#endif
    }

    (*real_exit)(status);
}

pid_t
monitor_real_fork(void)
{
    MONITOR_DEBUG("\n");

    MONITOR_TRY_SYMBOL(real_fork, "fork");
    if (real_fork == NULL) {
#if defined(SYS_fork)
	return syscall(SYS_fork);
#elif defined(__NR_fork)
	return syscall(__NR_fork);
#else
	MONITOR_ERROR("unable to find 'fork' or equivalent syscall\n");
#endif
    }

    return (*real_fork)();
}

int
monitor_real_execve PARAMS_EXECVE
{
    MONITOR_DEBUG("monitor_real_execve(%s, ...)\n", path);

    MONITOR_TRY_SYMBOL(real_execve, "execve");
    if (real_execve == NULL) {
#if defined(SYS_execve)
	return syscall(SYS_execve, path, argv, envp);
#elif defined(__NR_execve)
	return syscall(__NR_execve, path, argv, envp);
#else
	MONITOR_ERROR("unable to find 'execve' or equivalent syscall\n");
#endif
    }

    return (*real_execve)(path, argv, envp);
}

void *
monitor_real_dlopen(const char *filename, int flags)
{
    MONITOR_DEBUG("monitor_real_dlopen(%s, %d)\n", filename, flags);

    MONITOR_REQUIRE_SYMBOL(real_dlopen, "dlopen");

    return (*real_dlopen)(filename, flags);
}

/****************************************************************************
 * Intercepted routines
 ****************************************************************************/

/*
 *  Intercept the start routine: this is from glibc and can be one of
 *  two different names depending on how the macro BP_SYM is defined.
 *    glibc-x/sysdeps/generic/libc-start.c
 */

extern int
__libc_start_main PARAMS_START_MAIN
{
  monitor_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}


extern int
__BP___libc_start_main PARAMS_START_MAIN
{
  monitor_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}

/*
 *  Intercept creation of exec() family of routines because profiling
 *  *must* be off when the exec takes place or it interferes with
 *  ld.so (and the latter will not preload our monitoring library).
 *  Moreover, this allows us to safely write any profile data to disk.
 *
 *  execl, execlp, execle / execv, execvp, execve  
 *
 *  Note: We cannot simply intercept libc'c __execve (which implements
 *  all other routines on Linux) because it is a libc-local symbol.
 *    glibc-x/sysdeps/generic/execve.c
 * 
 *  Note: Stupid C has no way of passing along the input vararg list
 *  intact.
 */

extern int 
execl(const char *path, const char *arg, ...)
{
  const char** argv = NULL;
  va_list arglist;

  va_start(arglist, arg);
  monitor_parse_execl(&argv, NULL, arg, arglist);
  va_end(arglist);
  
  return monitor_execv(path, (char* const*)argv);
}

extern int 
execlp(const char *file, const char *arg, ...)
{
  const char** argv = NULL;
  va_list arglist;
  
  va_start(arglist, arg);
  monitor_parse_execl(&argv, NULL, arg, arglist);
  va_end(arglist);
  
  return monitor_execvp(file, (char* const*)argv);
}

extern int 
execle(const char *path, const char *arg, ...)
{
  const char** argv = NULL;
  const char* const* envp = NULL;
  va_list arglist;

  va_start(arglist, arg);
  monitor_parse_execl(&argv, &envp, arg, arglist);
  va_end(arglist);
  
  return monitor_execve(path, (char* const*)argv, (char* const*)envp);
}

extern int 
execv PARAMS_EXECV
{
  return monitor_execv(path, argv);
}

extern int 
execvp PARAMS_EXECVP
{
  return monitor_execvp(file, argv);
}

extern int 
execve PARAMS_EXECVE
{
  return monitor_execve(path, argv, envp);
}

/*
 *  Intercept creation of new processes via fork(), vfork()
 */

extern pid_t 
fork()
{
  return monitor_fork();
}


extern pid_t 
vfork()
{
  return monitor_fork();
}

/*
 *  Intercept premature exits that disregard registered atexit()
 *  handlers.
 *
 *    'normal' termination  : _exit, _Exit
 *    'abnormal' termination: abort
 *
 *  Note: _exit() implements _Exit().
 *  Note: We catch abort by catching SIGABRT
 */

extern void *dlopen(const char *filename, int flag)
{
    void *retval;

    MONITOR_DEBUG("dlopen(%s,%d)\n",filename,flag);

    retval = monitor_real_dlopen(filename,flag);
    /* Callback */

    if (retval && filename)
	monitor_dlopen(filename);

    return(retval);
}

extern void __attribute__ ((noreturn))
    _exit(int status)
{
    MONITOR_DEBUG("_exit(status = %d)\n", status);

    if ((monitor_opt_error & MONITOR_NONZERO_EXIT) || (status == 0))
	monitor_force_fini_process();
    MONITOR_FINI_LIBRARY;

    monitor__exit(status);
}

extern void
_Exit(int status)
{
    MONITOR_DEBUG("_Exit(status = %d)\n", status);

    _exit(status);
}

extern void
exit(int status)
{
    MONITOR_DEBUG("exit(status = %d)\n", status);

    if (!(monitor_opt_error & MONITOR_NONZERO_EXIT) && (status != 0))
    {
	/* This prevents callbacks from occuring inside of _fini */
	monitor_root_pid = 0;
    }
    monitor_real_exit(status);
}

/*
 *  Intercept creation of new theads via pthread_create()
 */
extern int
pthread_create PARAMS_PTHREAD_CREATE
{
    MONITOR_DEBUG("\n");

    return monitor_pthread_create(thread, attr, start_routine, arg);
}
