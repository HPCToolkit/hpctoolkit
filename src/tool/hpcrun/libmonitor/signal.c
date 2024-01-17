/*
 *  Libmonitor signal functions.
 *
 *  Copyright (c) 2007-2023, Rice University.
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
 *  $Id$
 */

#include "config.h"
#include <sys/types.h>
#ifdef MONITOR_DYNAMIC
#include <dlfcn.h>
#endif
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

typedef int  sigaction_fcn_t(int, const struct sigaction *,
			     struct sigaction *);
typedef int  sigprocmask_fcn_t(int, const sigset_t *, sigset_t *);
typedef void sighandler_fcn_t(int);

#ifdef MONITOR_STATIC
extern sigaction_fcn_t    __real_sigaction;
extern sigprocmask_fcn_t  __real_sigprocmask;
#endif

static sigaction_fcn_t    *real_sigaction = NULL;
static sigprocmask_fcn_t  *real_sigprocmask = NULL;

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
#ifdef MONITOR_BLUE_GENE_Q
    37,
#endif
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
    MONITOR_CLIENT_SIGNALS_LIST  -1
};

/*  Signals that monitor tries for thread shootdown.  We build this
 *  list in monitor_signal_init().
 */
static int monitor_shootdown_list[MONITOR_NSIG];

static int monitor_extra_list[] = {
    SIGUSR2, SIGUSR1, SIGPWR, SIGURG, SIGPIPE, -1
};

/*  Locks for the monitor_signal_array[].
 */
#define MONITOR_SIGNAL_LOCK    spinlock_lock(&monitor_signal_lock)
#define MONITOR_SIGNAL_UNLOCK  spinlock_unlock(&monitor_signal_lock)

static spinlock_t monitor_signal_lock = SPINLOCK_UNLOCKED;

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
#ifdef MONITOR_DYNAMIC
	monitor_end_library_fcn();
#endif
	action.sa_handler = SIG_DFL;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	(*real_sigaction)(sig, &action, NULL);
	(*real_sigprocmask)(SIG_SETMASK, &action.sa_mask, NULL);
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
 *  Install our signal hander for all signals except ones on the avoid
 *  list.
 */
void
monitor_signal_init(void)
{
    struct monitor_signal_entry *mse;
    struct sigaction *sa;
    char buf[MONITOR_SIG_BUF_SIZE];
    int num_avoid, num_valid, num_invalid;
    int i, sig, ret;

    MONITOR_RUN_ONCE(signal_init);
    MONITOR_GET_REAL_NAME_WRAP(real_sigaction, sigaction);
    MONITOR_GET_REAL_NAME_WRAP(real_sigprocmask, sigprocmask);

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
	    ret = (*real_sigaction)(sig, sa, &mse->mse_appl_act);
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
	(*real_sigprocmask)(0, NULL, &cur_set);

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
    sig = MONITOR_CONFIG_SHOOTDOWN_SIGNAL;
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
monitor_shootdown_signal(void)
{
    if (shootdown_signal > 0) {
	return shootdown_signal;
    }

    monitor_signal_init();
    monitor_choose_shootdown_early();
    return shootdown_signal;
}

#if 0
/*
 *  Old, delayed way of choosing the shootdown signal and keeping the
 *  client signals open.  The libunwind use case makes this approach
 *  of delaying the shootdown choice near impossible.
 */

/*
 *  Delete from "set" any signals on the keep open list.  Always leave
 *  at least one unmasked signal to use for thread shootdown.
 */
void
monitor_remove_client_signals(sigset_t *set)
{
    struct monitor_signal_entry *mse;
    int i, sig, old_avail, new_avail;

    if (set == NULL) {
	return;
    }

#if MONITOR_CHOOSE_SHOOTDOWN_EARLY

    for (sig = 1; sig < MONITOR_NSIG; sig++) {
	mse = &monitor_signal_array[sig];
	if (!mse->mse_avoid && !mse->mse_invalid && mse->mse_keep_open) {
	    sigdelset(set, sig);
	}
    }
    return;

#else
    /*
     * The old, delayed way of choosing the shootdown signal is broken
     * when using interrupts (deadlocks with sigprocmask).  This could
     * be fixed with a non-blocking data structure, but the libunwind
     * use case makes that pointless.
     */
    MONITOR_SIGNAL_LOCK;

    if (shootdown_signal < 0) {
	/*
	 * Try to find unblocked signals in the old and new sets.  The
	 * new mask could cover all available signals, but unless
	 * something strange happens, there should always at least one
	 * available signal in the old mask.
	 */
	old_avail = -1;
	new_avail = -1;
	for (i = 0; monitor_shootdown_list[i] > 0; i++) {
	    sig = monitor_shootdown_list[i];
	    mse = &monitor_signal_array[sig];
	    if (! mse->mse_blocked) {
		if (old_avail < 0) {
		    old_avail = sig;
		}
		if (sigismember(set, sig) == 0) {
		    new_avail = sig;
		    break;
		}
	    }
	}
	/*
	 * If the new mask covers all available signals, then choose
	 * the shootdown signal now and keep it open.
	 */
	if (old_avail > 0 && new_avail < 0) {
	    shootdown_signal = old_avail;
	    monitor_signal_array[old_avail].mse_keep_open = 1;
	}
    }
    /*
     * Remove the keep open signals from the set and update the
     * blocked list.
     */
    for (sig = 1; sig < MONITOR_NSIG; sig++) {
	mse = &monitor_signal_array[sig];
	if (!mse->mse_avoid && !mse->mse_invalid) {
	    if (mse->mse_keep_open) {
		sigdelset(set, sig);
	    }
	    else if (sigismember(set, sig) == 1) {
		mse->mse_blocked = 1;
	    }
	}
    }
    MONITOR_SIGNAL_UNLOCK;

#endif
}

/*
 *  Return a signal unused by the client or application, if possible.
 */
int
monitor_shootdown_signal(void)
{
    if (shootdown_signal > 0) {
	return shootdown_signal;
    }

#if MONITOR_CHOOSE_SHOOTDOWN_EARLY

    monitor_signal_init();
    monitor_choose_shootdown_early();
    return shootdown_signal;

#else

    struct monitor_signal_entry *mse;
    int i, sig, ans1, ans2, ans3;

    MONITOR_SIGNAL_LOCK;

    /*
     * Order of preference for the shootdown signal.
     *  1. unblocked and no client or application handler,
     *  2. unblocked and no client handler,
     *  3. any unblocked signal.
     */
    ans1 = -1;
    ans2 = -1;
    ans3 = -1;
    for (i = 0; monitor_shootdown_list[i] > 0; i++) {
	sig = monitor_shootdown_list[i];
	mse = &monitor_signal_array[sig];
	if (! mse->mse_blocked) {
	    if (ans3 < 0) {
		ans3 = sig;
	    }
	    if (mse->mse_client_handler == NULL) {
		if (ans2 < 0) {
		    ans2 = sig;
		}
		if (! mse->mse_appl_hand) {
		    ans1 = sig;
		    break;
		}
	    }
	}
    }
    if (ans1 > 0) {
	shootdown_signal = ans1;
    }
    else if (ans2 > 0) {
	shootdown_signal = ans2;
    }
    else if (ans3 > 0) {
	shootdown_signal = ans3;
    }
    else {
	/* Probably can never get here. */
	MONITOR_DEBUG("warning: unable to find satisfactory signal, "
		      "using: %d\n", last_resort_signal);
	shootdown_signal = last_resort_signal;
    }

    MONITOR_SIGNAL_UNLOCK;

    return shootdown_signal;

#endif
}

#endif  // end of old shootdown, client signals code


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
	(*real_sigaction)(sig, &mse->mse_kern_act, NULL);
    }

    return (0);
}

/*
 *  Client function for generating a core file.  Reset the default
 *  signal handler, clear the signal mask and raise SIGABRT.
 */
void
monitor_real_abort(void)
{
    struct sigaction action;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_sigaction, sigaction);
    MONITOR_GET_REAL_NAME_WRAP(real_sigprocmask, sigprocmask);

    action.sa_handler = SIG_DFL;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    (*real_sigaction)(SIGABRT, &action, NULL);
    (*real_sigprocmask)(SIG_SETMASK, &action.sa_mask, NULL);
    raise(SIGABRT);

    MONITOR_ERROR1("raise failed\n");
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
		       struct sigaction *oldact)
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
	return (*real_sigaction)(sig, act, oldact);
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
	(*real_sigaction)(sig, &mse->mse_kern_act, NULL);
    }

    return (0);
}

int
MONITOR_WRAP_NAME(sigaction)(int sig, const struct sigaction *act,
			     struct sigaction *oldact)
{
    return monitor_appl_sigaction(sig, act, oldact);
}

sighandler_fcn_t *
MONITOR_WRAP_NAME(signal)(int sig, sighandler_fcn_t *handler)
{
    struct sigaction act, oldact;

    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (monitor_appl_sigaction(sig, &act, &oldact) == 0)
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
MONITOR_WRAP_NAME(sigprocmask)(int how, const sigset_t *set,
			       sigset_t *oldset)
{
    sigset_t my_set;

    monitor_signal_init();

    if (set != NULL) {
	MONITOR_DEBUG1("\n");
	my_set = *set;
	monitor_remove_client_signals(&my_set, how);
	set = &my_set;
    }

    return (*real_sigprocmask)(how, set, oldset);
}
