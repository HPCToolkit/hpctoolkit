// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor fork and exec functions.
 *
 *  Override functions:
 *
 *    fork, vfork
 *    execl, execlp, execle, execv, execvp, execve
 *    system
 *
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "monitor.h"

#include "../audit/audit-api.h"

extern char **environ;

/*
 *----------------------------------------------------------------------
 *  GLOBAL VARIABLES and EXTERNAL FUNCTIONS
 *----------------------------------------------------------------------
 */

#define NO_SYSTEM_OVERRIDE  "MONITOR_NO_SYSTEM_OVERRIDE"

#define MONITOR_INIT_ARGV_SIZE  64
#define MONITOR_INIT_ENVIRON_SIZE  250
#define MONITOR_DEFAULT_PAGESIZE  4096

static char *newenv_array[MONITOR_INIT_ENVIRON_SIZE];

static int override_system = 1;

/*
 *----------------------------------------------------------------------
 *  INTERNAL HELPER FUNCTIONS
 *----------------------------------------------------------------------
 */

void
monitor_fork_init(void)
{
    static int init_done = 0;

    if (init_done)
        return;

    monitor_early_init();

    override_system = (getenv(NO_SYSTEM_OVERRIDE) == NULL);

    init_done = 1;
}

/*
 *  Copy the process's environment and omit LD_PRELOAD.
 *
 *  For most binaries, unsetenv(LD_PRELOAD) would work.  But inside
 *  bash, unsetenv() doesn't seem to change environ (why!?).  Also,
 *  copying environ is safer than modifying it in place if environ is
 *  somehow read-only.
 */
static char **
monitor_copy_environ(char *const oldenv[])
{
    char **newenv = &newenv_array[0];
    size_t pagesize = MONITOR_DEFAULT_PAGESIZE;
    size_t size;
    int k, n, prot, flags;

    /* Count the size of the old environ array. */
    for (n = 0; oldenv[n] != NULL; n++) {
    }
    n += 2;

    /* mmap() a larger array if needed. */
    if (n > MONITOR_INIT_ENVIRON_SIZE) {
#ifdef _SC_PAGESIZE
        if ((size = sysconf(_SC_PAGESIZE)) > 0) {
            pagesize = size;
        }
#endif
        size = n * sizeof(char *);
        size = ((size + pagesize - 1)/pagesize) * pagesize;
        prot = PROT_READ | PROT_WRITE;
#if defined(MAP_ANONYMOUS)
        flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
        flags = MAP_PRIVATE | MAP_ANON;
#endif
        newenv = mmap(NULL, size, prot, flags, -1, 0);
        if (newenv == MAP_FAILED) {
            MONITOR_ERROR("mmap failed, size: %ld, errno: %d\n",
                          size, errno);
        }
    }

    /* Copy old environ and omit LD_PRELOAD. */
    n = 0;
    for (k = 0; oldenv[k] != NULL; k++) {
        if (strstr(oldenv[k], "LD_PRELOAD") == NULL) {
            newenv[n] = oldenv[k];
            n++;
        }
    }
    newenv[n] = NULL;

    return newenv;
}

/*
 *  Approximate whether the real exec will succeed by testing if
 *  "file" is executable, either the pathname itself or somewhere on
 *  PATH, depending on whether file contains '/'.  The problem is that
 *  we have to decide whether to call monitor_fini_process() before we
 *  know if exec succeeds, since exec doesn't return on success.
 *
 *  Returns: 1 if "file" is executable, else 0.
 */
#define COLON  ":"
#define SLASH  '/'
static int
monitor_is_executable(const char *file)
{
    char buf[PATH_MAX], *path;
    int  file_len, path_len;

    if (file == NULL) {
        MONITOR_DEBUG1("attempt to exec NULL path\n");
        return (0);
    }
    /*
     * If file contains '/', then try just the file name itself,
     * otherwise, search for it on PATH.
     */
    if (strchr(file, SLASH) != NULL)
        return (access(file, X_OK) == 0);

    file_len = strlen(file);
    path = getenv("PATH");
    path += strspn(path, COLON);
    while (*path != 0) {
        path_len = strcspn(path, COLON);
        if (path_len + file_len + 2 > PATH_MAX) {
            MONITOR_DEBUG("path length %d exceeds PATH_MAX %d\n",
                          path_len + file_len + 2, PATH_MAX);
        }
        memcpy(buf, path, path_len);
        buf[path_len] = SLASH;
        memcpy(&buf[path_len + 1], file, file_len);
        buf[path_len + 1 + file_len] = 0;
        if (access(buf, X_OK) == 0)
            return (1);
        path += path_len;
        path += strspn(path, COLON);
    }

    return (0);
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
monitor_fork(const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    void *user_data;
    pid_t ret;

    monitor_fork_init();
    MONITOR_DEBUG1("calling monitor_pre_fork() ...\n");
    user_data = monitor_pre_fork();

    ret = f_fork(dispatch);
    if (ret != 0) {
        /* Parent process. */
        if (ret < 0) {
            MONITOR_DEBUG("real fork failed (%d): %s\n",
                          errno, strerror(errno));
        }
        MONITOR_DEBUG1("calling monitor_post_fork() ...\n");
        monitor_post_fork(ret, user_data);
    }
    else {
        /* Child process. */
        MONITOR_DEBUG("application forked, parent = %d\n", (int)getppid());
        monitor_begin_process_fcn(user_data, TRUE);
    }

    return (ret);
}

pid_t
hpcrun_fork(const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    return monitor_fork(dispatch);
}

pid_t
hpcrun_vfork(const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    return monitor_fork(dispatch);
}

/*
 *  Override execl() and execv().
 *
 *  Note: we test if "path" is executable as an approximation to
 *  whether exec will succeed, and thus whether we should call
 *  monitor_fini_process.  But we still try the real exec in either
 *  case.
 */
static int
monitor_execv(const char *path, char *const argv[],
              const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    int ret, is_exec;

    monitor_fork_init();
    is_exec = access(path, X_OK) == 0;
    MONITOR_DEBUG("about to execv, expecting %s, pid: %d, path: %s\n",
                  (is_exec ? "success" : "failure"),
                  (int)getpid(), path);
    if (is_exec) {
        monitor_end_process_fcn(MONITOR_EXIT_EXEC);
        monitor_end_library_fcn();
    }
    ret = f_execv(path, argv, dispatch);

    /* We only get here if real_execv fails. */
    if (is_exec) {
        MONITOR_WARN("unexpected execv failure on pid: %d\n",
                     (int)getpid());
    }
    return (ret);
}

/*
 *  Override execlp() and execvp().
 */
static int
monitor_execvp(const char *file, char *const argv[],
               const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    int ret, is_exec;

    monitor_fork_init();
    is_exec = monitor_is_executable(file);
    MONITOR_DEBUG("about to execvp, expecting %s, pid: %d, file: %s\n",
                  (is_exec ? "success" : "failure"),
                  (int)getpid(), file);
    if (is_exec) {
        monitor_end_process_fcn(MONITOR_EXIT_EXEC);
        monitor_end_library_fcn();
    }
    ret = f_execvp(file, argv, dispatch);

    /* We only get here if real_execvp fails. */
    if (is_exec) {
        MONITOR_WARN("unexpected execvp failure on pid: %d\n",
                     (int)getpid());
    }
    return (ret);
}

/*
 *  Override execle() and execve().
 */
static int
monitor_execve(const char *path, char *const argv[], char *const envp[],
               const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    int ret, is_exec;

    monitor_fork_init();
    is_exec = access(path, X_OK) == 0;
    MONITOR_DEBUG("about to execve, expecting %s, pid: %d, path: %s\n",
                  (is_exec ? "success" : "failure"),
                  (int)getpid(), path);
    if (is_exec) {
        monitor_end_process_fcn(MONITOR_EXIT_EXEC);
        monitor_end_library_fcn();
    }
    ret = f_execve(path, argv, envp, dispatch);

    /* We only get here if real_execve fails. */
    if (is_exec) {
        MONITOR_WARN("unexpected execve failure on pid: %d\n",
                     (int)getpid());
    }
    return (ret);
}


int
hpcrun_execv(const char *path, char *const argv[],
               const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    return monitor_execv(path, argv, dispatch);
}

int
hpcrun_execvp(const char *path, char *const argv[],
                const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    return monitor_execvp(path, argv, dispatch);
}

int
hpcrun_execve(const char *path, char *const argv[], char *const envp[],
                const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    return monitor_execve(path, argv, envp, dispatch);
}

/*
 *  Reimplement system() for application override (with callback
 *  functions) and client support (without callbacks).  Stevens
 *  describes the issues with signals and how to do this.
 *
 *  This allows us to do three things: (1) replace vfork() with fork,
 *  (2) provide pre/post_fork() callbacks, and (3) selectively monitor
 *  or not the child process.  Note: the libc system() does a direct
 *  syscall for fork, thus bypassing our override.
 */
#define SHELL  "/bin/sh"
static int
monitor_system(const char *command, int callback,
               const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    struct sigaction ign_act, old_int, old_quit;
    sigset_t sigchld_set, old_set;
    void *user_data = NULL;
    char *arglist[4];
    char *who;
    pid_t pid;
    int status;

    monitor_fork_init();
    who = (callback ? "appl" : "client");
    MONITOR_DEBUG("(%s) command = %s\n", who, command);
    /*
     * command == NULL tests if the shell is available and returns
     * non-zero if yes.
     */
    if (command == NULL) {
        status = !access(SHELL, X_OK);
        MONITOR_DEBUG("(%s) status = %d\n", who, status);
        return (status);
    }
    /*
     * Ignore SIGINT and SIGQUIT, and block SIGCHLD.
     */
    memset(&ign_act, 0, sizeof(struct sigaction));
    ign_act.sa_handler = SIG_IGN;
    sigemptyset(&sigchld_set);
    sigaddset(&sigchld_set, SIGCHLD);

    if (callback) {
        MONITOR_DEBUG("(%s) calling monitor_pre_fork() ...\n", who);
        user_data = monitor_pre_fork();
    }
    auditor_exports()->sigaction(SIGINT, &ign_act, &old_int);
    auditor_exports()->sigaction(SIGQUIT, &ign_act, &old_quit);
    auditor_exports()->sigprocmask(SIG_BLOCK, &sigchld_set, &old_set);

    pid = f_fork(dispatch);
    if (pid < 0) {
        /* Fork failed. */
        MONITOR_DEBUG("(%s) real fork failed\n", who);
        status = -1;
    }
    else if (pid == 0) {
        /* Child process. */
        MONITOR_DEBUG("(%s) child process about to exec, parent = %d\n",
                      who, getppid());
        auditor_exports()->sigaction(SIGINT, &old_int, NULL);
        auditor_exports()->sigaction(SIGQUIT, &old_quit, NULL);
        auditor_exports()->sigprocmask(SIG_SETMASK, &old_set, NULL);
        arglist[0] = SHELL;
        arglist[1] = "-c";
        arglist[2] = (char *)command;
        arglist[3] = NULL;
        f_execve(SHELL, arglist,
                 callback ? environ : monitor_copy_environ(environ), dispatch);
        auditor_exports()->exit(127);
        __builtin_unreachable();
    }
    else {
        /* Parent process. */
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) {
                status = -1;
                break;
            }
        }
    }
    auditor_exports()->sigaction(SIGINT, &old_int, NULL);
    auditor_exports()->sigaction(SIGQUIT, &old_quit, NULL);
    auditor_exports()->sigprocmask(SIG_SETMASK, &old_set, NULL);

    if (callback) {
        MONITOR_DEBUG("(%s) calling monitor_post_fork() ...\n", who);
        monitor_post_fork(pid, user_data);
    }

    MONITOR_DEBUG("(%s) status = %d\n", who, status);
    return (status);
}

int
hpcrun_system(const char *command,
                const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    int ret;

    monitor_fork_init();

    if (override_system) {
        ret = monitor_system(command, TRUE, dispatch);
    }
    else {
        MONITOR_DEBUG("system (no override): %s\n", command);
        ret = f_system(command, dispatch);
    }

    return ret;
}
