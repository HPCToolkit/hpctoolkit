// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor signal functions.
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "monitor.h"
#include "spinlock.h"
#include "../audit/audit-api.h"

#define MONITOR_CHOOSE_SHOOTDOWN_EARLY  1

/*
 *----------------------------------------------------------------------
 *  MACROS and GLOBAL DEFINITIONS
 *----------------------------------------------------------------------
 */

/*  Sa_flags that monitor requires and forbids.
 */
#define SAFLAGS_REQUIRED   (SA_SIGINFO | SA_RESTART)
#define SAFLAGS_FORBIDDEN  (SA_RESETHAND | SA_ONSTACK)

struct monitor_signal_entry {
    struct sigaction  mse_appl_act;
    struct sigaction  mse_kern_act;
    monitor_sighandler_t  *mse_client_handler;
    int   mse_client_flags;
    char  mse_avoid;
    char  mse_invalid;
    char  mse_noterm;
    char  mse_noraise;
    char  mse_stop;
    char  mse_blocked;
    char  mse_appl_hand;
    char  mse_keep_open;
};

static struct monitor_signal_entry
monitor_signal_array[MONITOR_NSIG];

/*  Signals that monitor treats as totally hands-off.
 */
static int monitor_signal_avoid_list[] = {
    SIGKILL, SIGSTOP, -1
};

/*  Signals whose default actions do not terminate the process.
 *  On BG/Q, sig 37 = SIGMUFIFOFULL is non-terminating.
 */
static int monitor_signal_noterm_list[] = {
    SIGCHLD, SIGCONT, SIGURG, SIGWINCH, -1
};

/*  Synchronous signals where we terminate the process by returning
 *  from the signal handler instead of reraising.  This gives a better
 *  stack trace for signals like segv, etc.
 */
static int monitor_signal_noraise_list[] = {
    SIGSEGV, SIGBUS, SIGILL, -1
};

/*  Signals whose default action is to stop the process.
 */
static int monitor_signal_stop_list[] = {
    SIGTSTP, SIGTTIN, SIGTTOU, -1
};

/*  Signals that the application is not allowed to block.  This list
 *  comes from the --enable-client-signals option in configure.
 */
static int monitor_signal_open_list[] = {
    SIGBUS, SIGSEGV, SIGPROF, 36, 37, 38, -1
};

/*  Signals that monitor tries for thread shootdown.  We build this
 *  list in monitor_signal_init().
 */
static int monitor_shootdown_list[MONITOR_NSIG];

static int monitor_extra_list[] = {
    SIGUSR2, SIGUSR1, SIGPWR, SIGURG, SIGPIPE, -1
};

static int last_resort_signal = SIGWINCH;
static int shootdown_signal = -1;

static void monitor_choose_shootdown_early(void);
static inline int monitor_adjust_samask(sigset_t *);

/*
 *----------------------------------------------------------------------
 *  INTERNAL HELPER FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Offer a synchronous signal from sigwait() to the client.
 *
 *  Returns: 0 if the client accepts the signal, ie, nothing left to
 *  do (same as handler in monitor_sigaction).
 */
int
monitor_sigwait_handler(int sig, siginfo_t *info, void *context)
{
    struct monitor_signal_entry *mse;
    int ret;

    monitor_signal_init();

    if (sig <= 0 || sig >= MONITOR_NSIG) {
        return 1;
    }

    mse = &monitor_signal_array[sig];
    if (mse->mse_client_handler != NULL) {
        ret = (mse->mse_client_handler)(sig, info, context);
        if (ret == 0) {
            return 0;
        }
    }

    return 1;
}

/*
 *  Catch all signals here.  Offer them to the client first (if
 *  registered), or else apply the application's disposition.
 */
static void
monitor_signal_handler(int sig, siginfo_t *info, void *context)
{
    struct monitor_signal_entry *mse;
    struct sigaction action, *sa;
    int ret;

    if (sig <= 0 || sig >= MONITOR_NSIG ||
        monitor_signal_array[sig].mse_avoid ||
        monitor_signal_array[sig].mse_invalid)
    {
        MONITOR_WARN("invalid signal: %d\n", sig);
        return;
    }

    /*
     * Try the client first, if it has registered a handler.  If the
     * client returns, then its return code describes what further
     * action to take, where 0 means no further action.
     */
    mse = &monitor_signal_array[sig];
    if (mse->mse_client_handler != NULL) {
        ret = (mse->mse_client_handler)(sig, info, context);
        if (ret == 0) {
            return;
        }
    }

    /*
     * Apply the applicaton's action: ignore, default or handler.
     */
    sa = &mse->mse_appl_act;
    if (sa == NULL || sa->sa_handler == SIG_IGN) {
        /*
         * Ignore the signal.
         */
        return;
    }
    else if (sa->sa_handler == SIG_DFL) {
        /*
         * Invoke the default action: ignore the signal, stop the
         * process, or (by default) terminate the process.
         *
         * XXX - may want to siglongjmp() or _exit() directly before
         * ending the process, or may want to restart some signals
         * (SIGSEGV, SIGFPE, etc).
         */
        if (mse->mse_noterm)
            return;

        if (mse->mse_stop) {
            raise(SIGSTOP);
            return;
        }

        monitor_end_process_fcn(MONITOR_EXIT_SIGNAL);
        monitor_end_library_fcn();
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
        sigemptyset(&action.sa_mask);
        auditor_exports()->sigaction(sig, &action, NULL);
        auditor_exports()->sigprocmask(SIG_SETMASK, &action.sa_mask, NULL);
        if (mse->mse_noraise) {
            /* For segv, etc, we just return. */
            return;
        }
        raise(sig);
    }
    else {
        /*
         * Invoke the application's handler.
         */
        if (sa->sa_flags & SA_SIGINFO) {
            (*sa->sa_sigaction)(sig, info, context);
        } else {
            (*sa->sa_handler)(sig);
        }
    }
}

/*
 *  Install our signal handler for all signals except ones on the avoid
 *  list.
 */
void
monitor_signal_init()
{
    struct monitor_signal_entry *mse;
    struct sigaction *sa;
    char buf[MONITOR_SIG_BUF_SIZE];
    int num_avoid, num_valid, num_invalid;
    int i, sig, ret;

    MONITOR_RUN_ONCE(signal_init);

    memset(monitor_signal_array, 0, sizeof(monitor_signal_array));
    for (i = 0; monitor_signal_avoid_list[i] > 0; i++) {
        monitor_signal_array[monitor_signal_avoid_list[i]].mse_avoid = 1;
    }
    for (i = 0; monitor_signal_noterm_list[i] > 0; i++) {
        monitor_signal_array[monitor_signal_noterm_list[i]].mse_noterm = 1;
    }
    for (i = 0; monitor_signal_noraise_list[i] > 0; i++) {
        monitor_signal_array[monitor_signal_noraise_list[i]].mse_noraise = 1;
    }
    for (i = 0; monitor_signal_stop_list[i] > 0; i++) {
        monitor_signal_array[monitor_signal_stop_list[i]].mse_stop = 1;
    }
    for (i = 0; monitor_signal_open_list[i] > 0; i++) {
        sig = monitor_signal_open_list[i];
        if (1 <= sig && sig < MONITOR_NSIG) {
            monitor_signal_array[sig].mse_keep_open = 1;
        }
    }

    monitor_choose_shootdown_early();
    monitor_signal_array[shootdown_signal].mse_keep_open = 1;

    /*
     * Install our signal handler for all signals.
     */
    num_avoid = 0;
    num_valid = 0;
    num_invalid = 0;
    for (sig = 1; sig < MONITOR_NSIG; sig++) {
        mse = &monitor_signal_array[sig];
        if (mse->mse_avoid) {
            num_avoid++;
        } else {
            sa = &mse->mse_kern_act;
            sa->sa_sigaction = &monitor_signal_handler;
            sigemptyset(&sa->sa_mask);
            monitor_adjust_samask(&sa->sa_mask);
            sa->sa_flags = SAFLAGS_REQUIRED;
            ret = auditor_exports()->sigaction(sig, sa, &mse->mse_appl_act);
            if (ret == 0) {
                num_valid++;
            } else {
                mse->mse_invalid = 1;
                num_invalid++;
            }
        }
    }

    if (monitor_debug) {
        MONITOR_DEBUG("valid: %d, invalid: %d, avoid: %d, max signum: %d\n",
                      num_valid, num_invalid, num_avoid, MONITOR_NSIG - 1);

        monitor_signal_list_string(buf, MONITOR_SIG_BUF_SIZE,
                                   monitor_signal_open_list);
        MONITOR_DEBUG("client list:%s\n", buf);

        monitor_signal_list_string(buf, MONITOR_SIG_BUF_SIZE,
                                   monitor_shootdown_list);
        MONITOR_DEBUG("shootdown list:%s\n", buf);
        MONITOR_DEBUG("shootdown signal: %d\n", shootdown_signal);
    }
}

/*
 *----------------------------------------------------------------------
 *  SHOOTDOWN SIGNAL and helper functions
 *----------------------------------------------------------------------
 */

/*
 *  Adjust the signal set so the application can't change the mask for
 *  any signal in the keep open list.
 */
void
monitor_remove_client_signals(sigset_t *set, int how)
{
    struct monitor_signal_entry *mse;
    char buf[MONITOR_SIG_BUF_SIZE];
    char *type = "";
    int sig;

    if (set == NULL) {
        return;
    }

    if (monitor_debug) {
        if (how == SIG_BLOCK) { type = "block"; }
        else if (how == SIG_UNBLOCK) { type = "unblock"; }
        else { type = "setmask"; }

        monitor_sigset_string(buf, MONITOR_SIG_BUF_SIZE, set);
        MONITOR_DEBUG("(%s) request:%s\n", type, buf);
    }

    if (how == SIG_BLOCK || how == SIG_UNBLOCK) {
        /*
         * For BLOCK and UNBLOCK, remove from 'set' any signals in the
         * keep open list.
         */
        for (sig = 1; sig < MONITOR_NSIG; sig++) {
            mse = &monitor_signal_array[sig];
            if (!mse->mse_avoid && !mse->mse_invalid && mse->mse_keep_open) {
                sigdelset(set, sig);
            }
        }
    }
    else {
        /*
         * For SETMASK, get the current mask and require that 'set'
         * has the same value for all signals in the keep open list.
         */
        sigset_t cur_set;
        auditor_exports()->sigprocmask(0, NULL, &cur_set);

        if (monitor_debug) {
            monitor_sigset_string(buf, MONITOR_SIG_BUF_SIZE, &cur_set);
            MONITOR_DEBUG("(%s) current:%s\n", type, buf);
        }

        for (sig = 1; sig < MONITOR_NSIG; sig++) {
            mse = &monitor_signal_array[sig];
            if (!mse->mse_avoid && !mse->mse_invalid && mse->mse_keep_open) {
                if (sigismember(&cur_set, sig)) {
                    sigaddset(set, sig);
                } else {
                    sigdelset(set, sig);
                }
            }
        }
    }

    if (monitor_debug) {
        monitor_sigset_string(buf, MONITOR_SIG_BUF_SIZE, set);
        MONITOR_DEBUG("(%s) actual: %s\n", type, buf);
    }
}

/*
 *  Choose the shootdown signal and add it to the open list.  In the
 *  early case, pick something on the shootdown list that's not held
 *  open for the client.
 */
static void
monitor_choose_shootdown_early(void)
{
    struct monitor_signal_entry *mse;
    char *shootdown_str;
    int i, k, sig;

    if (shootdown_signal > 0) {
        return;
    }

    /*
     * Build the shootdown avail list: the real-time signals first
     * followed by the extra list.  SIGRTMIN expands to a syscall, so
     * we have to build the list at runtime.
     */
    k = 0;
#ifdef SIGRTMIN
    for (sig = SIGRTMIN + 8; sig <= SIGRTMAX - 8; sig++) {
        mse = &monitor_signal_array[sig];
        if (sig < MONITOR_NSIG
            && !mse->mse_avoid && !mse->mse_invalid
            && !mse->mse_stop  && !mse->mse_keep_open)
        {
            monitor_shootdown_list[k] = sig;
            k++;
        }
    }
#endif
    for (i = 0; monitor_extra_list[i] > 0; i++) {
        sig = monitor_extra_list[i];
        mse = &monitor_signal_array[sig];
        if (!mse->mse_avoid   && !mse->mse_invalid
            && !mse->mse_stop && !mse->mse_keep_open)
        {
            monitor_shootdown_list[k] = sig;
            k++;
        }
    }
    monitor_shootdown_list[k] = last_resort_signal;
    monitor_shootdown_list[k + 1] = -1;

    /*
     * Allow MONITOR_SHOOTDOWN_SIGNAL to set the shootdown signal.
     * If set, this always has first priority.
     */
    shootdown_str = getenv("MONITOR_SHOOTDOWN_SIGNAL");
    if (shootdown_str != NULL) {
        if (sscanf(shootdown_str, "%d", &sig) == 1
            && sig > 0 && sig < MONITOR_NSIG
            && !monitor_signal_array[sig].mse_avoid
            && !monitor_signal_array[sig].mse_invalid
            && !monitor_signal_array[sig].mse_stop)
        {
            shootdown_signal = sig;
            MONITOR_DEBUG("shootdown signal (environ) = %d\n", shootdown_signal);
            return;
        }
    }

    /*
     * See if this was set in configure.
     */
    sig = 0;
    if (sig > 0 && sig < MONITOR_NSIG
        && !monitor_signal_array[sig].mse_avoid
        && !monitor_signal_array[sig].mse_invalid
        && !monitor_signal_array[sig].mse_stop)
    {
        shootdown_signal = sig;
        MONITOR_DEBUG("shootdown signal (config) = %d\n", shootdown_signal);
        return;
    }

    /*
     * Choose something from the shootdown list.
     */
    for (i = 0; monitor_shootdown_list[i] > 0; i++) {
        sig = monitor_shootdown_list[i];

        if (! monitor_signal_array[sig].mse_keep_open) {
            shootdown_signal = sig;
            MONITOR_DEBUG("shootdown signal (list) = %d\n", shootdown_signal);
            return;
        }
    }

    shootdown_signal = last_resort_signal;
    MONITOR_DEBUG("shootdown signal (last resort) = %d\n", shootdown_signal);
}

/*
 *  Return a signal unused by the client or application, if possible.
 */
int
monitor_shootdown_signal(const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    if (shootdown_signal > 0) {
        return shootdown_signal;
    }

    monitor_signal_init(dispatch);
    monitor_choose_shootdown_early();
    return shootdown_signal;
}

/*
 *----------------------------------------------------------------------
 *  SUPPORT FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Adjust sa_flags according to the required and forbidden sets.
 */
static inline int
monitor_adjust_saflags(int flags)
{
    return (flags | SAFLAGS_REQUIRED) & ~(SAFLAGS_FORBIDDEN);
}

/*
 *  Adjust sa_mask to block the shootdown signal, so we don't call
 *  fini-thread from inside another handler.  This is only for calls
 *  to sigaction(), not sigprocmask().
 */
static inline int
monitor_adjust_samask(sigset_t *set)
{
    if (set == NULL) {
        return 0;
    }
    return sigaddset(set, shootdown_signal);
}

/*
 *  The client's sigaction.  If "act" is non-NULL, then use it for
 *  sa_flags and sa_mask, but "flags" are unused for now.
 *
 *  Returns: 0 on success, -1 if invalid signal number.
 */
int
monitor_sigaction(int sig, monitor_sighandler_t *handler,
                  int flags, struct sigaction *act)
{
    struct monitor_signal_entry *mse;

    monitor_signal_init();
    if (sig <= 0 || sig >= MONITOR_NSIG ||
        monitor_signal_array[sig].mse_avoid ||
        monitor_signal_array[sig].mse_invalid) {
        MONITOR_DEBUG("client sigaction: %d (invalid)\n", sig);
        return (-1);
    }
    mse = &monitor_signal_array[sig];
    mse->mse_keep_open = 1;

    MONITOR_DEBUG("client sigaction: %d (caught)\n", sig);
    mse->mse_client_handler = handler;
    mse->mse_client_flags = flags;
    if (act != NULL) {
        mse->mse_kern_act.sa_flags = monitor_adjust_saflags(act->sa_flags);
        mse->mse_kern_act.sa_mask = act->sa_mask;
        monitor_adjust_samask(&mse->mse_kern_act.sa_mask);
        auditor_exports()->sigaction(sig, &mse->mse_kern_act, NULL);
    }

    return (0);
}

/*
 *----------------------------------------------------------------------
 *  EXTERNAL OVERRIDES and their helper functions
 *----------------------------------------------------------------------
 */

/*
 *  Override the application's signal(2) and sigaction(2).
 *
 *  Returns: 0 on success, -1 if invalid signal number.
 */
static int
monitor_appl_sigaction(int sig, const struct sigaction *act,
    struct sigaction *oldact, const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    struct monitor_signal_entry *mse;
    char *action;

    monitor_signal_init();
    if (sig <= 0 || sig >= MONITOR_NSIG ||
        monitor_signal_array[sig].mse_invalid) {
        MONITOR_DEBUG("application sigaction: %d (invalid)\n", sig);
        errno = EINVAL;
        return (-1);
    }
    mse = &monitor_signal_array[sig];

    /*
     * Use the system sigaction for signals on the avoid list.
     */
    if (mse->mse_avoid) {
        MONITOR_DEBUG("application sigaction: %d (avoid)\n", sig);
        return f_sigaction(sig, act, oldact, dispatch);
    }

    if (act == NULL) {
        action = "null";
    } else if (act->sa_handler == SIG_DFL) {
        action = "default";
    } else if (act->sa_handler == SIG_IGN) {
        action = "ignore";
    } else {
        action = "caught";
        mse->mse_appl_hand = 1;
    }
    MONITOR_DEBUG("application sigaction: %d (%s)\n", sig, action);

    if (oldact != NULL) {
        *oldact = mse->mse_appl_act;
    }
    if (act != NULL) {
        /*
         * Use the application's flags and mask, adjusted for
         * monitor's needs.
         */
        mse->mse_appl_act = *act;
        mse->mse_kern_act.sa_flags = monitor_adjust_saflags(act->sa_flags);
        mse->mse_kern_act.sa_mask = act->sa_mask;
        monitor_remove_client_signals(&mse->mse_kern_act.sa_mask, SIG_BLOCK);
        monitor_adjust_samask(&mse->mse_kern_act.sa_mask);
        f_sigaction(sig, &mse->mse_kern_act, NULL, dispatch);
    }

    return (0);
}

int
hpcrun_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact,
                   const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    return monitor_appl_sigaction(sig, act, oldact, dispatch);
}

typedef void sighandler_fcn_t(int);
sighandler_fcn_t *
hpcrun_signal(int sig, sighandler_fcn_t *handler,
                const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    struct sigaction act, oldact;

    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (monitor_appl_sigaction(sig, &act, &oldact, dispatch) == 0)
        return (oldact.sa_handler);
    else {
        errno = EINVAL;
        return (SIG_ERR);
    }
}

/*
 *  Allow the application to modify the signal mask, but don't let it
 *  change the mask for any signal in the keep open list.
 */
int
hpcrun_sigprocmask(int how, const sigset_t *set,
                     sigset_t *oldset, const struct hpcrun_foil_appdispatch_libc* dispatch)
{
    sigset_t my_set;

    monitor_signal_init();

    if (set != NULL) {
        MONITOR_DEBUG1("\n");
        my_set = *set;
        monitor_remove_client_signals(&my_set, how);
        set = &my_set;
    }

    return f_sigprocmask(how, set, oldset, dispatch);
}
