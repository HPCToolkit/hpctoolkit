/*
 * Monitor fork and exec functions.
 *
 * $Id$
 */

#include "config.h"
#include <sys/types.h>
#ifdef MONITOR_DYNAMIC
#include <dlfcn.h>
#endif
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "monitor.h"

/*
 *----------------------------------------------------------------------
 *  MONITOR FORK, EXEC DEFINITIONS
 *----------------------------------------------------------------------
 */

typedef pid_t fork_fcn_t(void);
typedef int execv_fcn_t(const char *path, char *const argv[]);
typedef int execve_fcn_t(const char *path, char *const argv[],
			 char *const envp[]);

#ifdef MONITOR_STATIC
extern fork_fcn_t    __real_fork;
extern execv_fcn_t   __real_execv;
extern execv_fcn_t   __real_execvp;
extern execve_fcn_t  __real_execve;
#endif

static fork_fcn_t    *real_fork = NULL;
static execv_fcn_t   *real_execv = NULL;
static execv_fcn_t   *real_execvp = NULL;
static execve_fcn_t  *real_execve = NULL;

/*
 *----------------------------------------------------------------------
 *  INTERNAL HELPER FUNCTIONS
 *----------------------------------------------------------------------
 */
static void
monitor_fork_init(void)
{
    MONITOR_RUN_ONCE(fork_init);

    monitor_early_init();
    MONITOR_GET_REAL_NAME_WRAP(real_fork, fork);
    MONITOR_GET_REAL_NAME_WRAP(real_execv, execv);
    MONITOR_GET_REAL_NAME_WRAP(real_execvp, execvp);
    MONITOR_GET_REAL_NAME_WRAP(real_execve, execve);
}

/*
 *  Copy the execl() argument list of first_arg followed by arglist
 *  into an argv array, including the terminating NULL.  If envp is
 *  non-NULL, then there is an extra argument after NULL.  va_start
 *  and va_end are in the calling function.
 */
#define MONITOR_INIT_ARGV_SIZE  32
static void
monitor_copy_va_args(char ***argv, char ***envp,
		     const char *first_arg, va_list arglist)
{
    int argc, size = MONITOR_INIT_ARGV_SIZE;
    char *arg;

    *argv = malloc(size * sizeof(char *));
    if (*argv == NULL) {
	MONITOR_ERROR1("malloc failed\n");
    }

    /*
     * Include the terminating NULL in the argv array.
     */
    (*argv)[0] = (char *)first_arg;
    argc = 1;
    do {
	arg = va_arg(arglist, char *);
	if (argc >= size) {
	    size *= 2;
	    *argv = realloc(*argv, size * sizeof(char *));
	    if (*argv == NULL) {
		MONITOR_ERROR1("realloc failed\n");
	    }
	}
	(*argv)[argc++] = arg;
    } while (arg != NULL);

    if (envp != NULL) {
	*envp = va_arg(arglist, char **);
    }
}

/*
 *----------------------------------------------------------------------
 *  FORK and EXEC OVERRIDE FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Override fork() and vfork().
 *
 *  Note: vfork must be immediately followed by exec and can't even
 *  return from our override function.  Thus, we have to use the real
 *  fork (not the real vfork) in both cases.
 */
static pid_t
monitor_fork(void)
{
    pid_t ret;

    monitor_fork_init();
    ret = (*real_fork)();
    if (ret < 0) {
	/* Failure. */
	MONITOR_DEBUG("real fork failed (%d): %s\n",
		      errno, strerror(errno));
    }
    else if (ret == 0) {
	/* Child process. */
	MONITOR_DEBUG("application forked, new pid = %d\n",
		      (int)getpid());
	monitor_begin_process_fcn();
    }

    return (ret);
}

pid_t
MONITOR_WRAP_NAME(fork)(void)
{
    return monitor_fork();
}

pid_t
MONITOR_WRAP_NAME(vfork)(void)
{
    return monitor_fork();
}

/*
 *  Override execl() and execv().
 */
static int
monitor_execv(const char *path, char *const argv[])
{
    int ret;

    monitor_fork_init();
    /*
     * Approximate whether real exec will succeed by testing if the
     * file is executable.
     */
    if (access(path, X_OK) < 0) {
	MONITOR_DEBUG("attempt to exec non-executable path: %s\n", path);
	errno = EACCES;
	return (-1);
    }
    MONITOR_DEBUG("about to execv, pid: %d, path: %s\n",
		  (int)getpid(), path);

    monitor_end_process_fcn();
#ifdef MONITOR_DYNAMIC
    monitor_end_library_fcn();
#endif

    ret = (*real_execv)(path, argv);
    MONITOR_WARN("real execv failed on pid: %d\n", (int)getpid());

    /* We only get here if real_execv fails. */
    return (ret);
}

/*
 *  Override execlp() and execvp().
 */
static int
monitor_execvp(const char *file, char *const argv[])
{
    int ret;

    monitor_fork_init();
    /*
     * FIXME - Walk PATH to see if file is executable.
     * For now, we just assume that execvp succeeds.
     */
    MONITOR_DEBUG("about to execvp, pid: %d, file: %s\n",
		  (int)getpid(), file);

    monitor_end_process_fcn();
#ifdef MONITOR_DYNAMIC
    monitor_end_library_fcn();
#endif

    ret = (*real_execvp)(file, argv);
    MONITOR_WARN("real execvp failed on pid %d\n", (int)getpid());

    /* We only get here if real_execvp fails. */
    return (ret);
}

/*
 *  Override execle() and execve().
 */
static int
monitor_execve(const char *path, char *const argv[], char *const envp[])
{
    int ret;

    monitor_fork_init();
    /*
     * Approximate whether real exec will succeed by testing if the
     * file is executable.
     */
    if (access(path, X_OK) < 0) {
	MONITOR_DEBUG("attempt to exec non-executable path: %s\n", path);
	errno = EACCES;
	return (-1);
    }
    MONITOR_DEBUG("about to execve, pid: %d, path: %s\n",
		  (int)getpid(), path);

    monitor_end_process_fcn();
#ifdef MONITOR_DYNAMIC
    monitor_end_library_fcn();
#endif

    ret = (*real_execve)(path, argv, envp);
    MONITOR_WARN("real execve failed on pid %d\n", (int)getpid());

    /* We only get here if real_execve fails. */
    return (ret);
}

/*
 *  There's a subtlety in the signatures between the variadic args in
 *  execl() and the argv array in execv().
 *
 *    execl(const char *path, const char *arg, ...)
 *    execv(const char *path, char *const argv[])
 *
 *  In execv(), the elements of argv[] are const, but not the strings
 *  they point to.  Since we can't change the system declaratons,
 *  somewhere we have to drop the const.  (ugh)
 */
int
MONITOR_WRAP_NAME(execl)(const char *path, const char *arg, ...)
{
    char **argv;
    va_list arglist;

    va_start(arglist, arg);
    monitor_copy_va_args(&argv, NULL, arg, arglist);
    va_end(arglist);

    return monitor_execv(path, argv);
}

int
MONITOR_WRAP_NAME(execlp)(const char *file, const char *arg, ...)
{
    char **argv;
    va_list arglist;

    va_start(arglist, arg);
    monitor_copy_va_args(&argv, NULL, arg, arglist);
    va_end(arglist);

    return monitor_execvp(file, argv);
}

int
MONITOR_WRAP_NAME(execle)(const char *path, const char *arg, ...)
{
    char **argv, **envp;
    va_list arglist;

    va_start(arglist, arg);
    monitor_copy_va_args(&argv, &envp, arg, arglist);
    va_end(arglist);

    return monitor_execve(path, argv, envp);
}

int
MONITOR_WRAP_NAME(execv)(const char *path, char *const argv[])
{
    return monitor_execv(path, argv);
}

int
MONITOR_WRAP_NAME(execvp)(const char *path, char *const argv[])
{
    return monitor_execvp(path, argv);
}

int
MONITOR_WRAP_NAME(execve)(const char *path, char *const argv[],
			  char *const envp[])
{
    return monitor_execve(path, argv, envp);
}
