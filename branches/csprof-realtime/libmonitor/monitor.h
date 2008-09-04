/*
 *  Include file for libmonitor clients.
 *
 *  Copyright (c) 2007-2008, Rice University.
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
 *  $Id: monitor.h 89 2008-05-13 19:01:09Z krentel $
 */

#ifndef  _MONITOR_H_
#define  _MONITOR_H_

#include <sys/types.h>
#include <signal.h>

typedef int monitor_sighandler_t(int, siginfo_t *, void *);

enum { MONITOR_EXIT_NORMAL = 1, MONITOR_EXIT_SIGNAL, MONITOR_EXIT_EXEC };

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Callback functions for the client to override.
 */
extern void monitor_init_library(void);
extern void monitor_fini_library(void);
extern void *monitor_init_process(int *argc, char **argv, void *data);
extern void monitor_fini_process(int how, void *data);
extern void *monitor_pre_fork(void);
extern void monitor_post_fork(pid_t child, void *data);
extern void *monitor_thread_pre_create(void);
extern void monitor_thread_post_create(void *);
extern void monitor_init_thread_support(void);
extern void *monitor_init_thread(int tid, void *data);
extern void monitor_fini_thread(void *data);
extern void monitor_pre_dlopen(const char *path, int flags);
extern void monitor_dlopen(const char *path, int flags, void *handle);
extern void monitor_dlclose(void *handle);
extern void monitor_post_dlclose(void *handle, int ret);
extern void monitor_init_mpi(int *argc, char ***argv);
extern void monitor_fini_mpi(void);

/*
 *  Monitor support functions.
 */
extern void monitor_real_abort(void);
extern void monitor_real_exit(int status);
extern pid_t monitor_real_fork(void);
extern int monitor_real_system(const char *command);
extern void *monitor_real_dlopen(const char *path, int flags);
extern int monitor_real_dlclose(void *handle);
extern int monitor_real_sigprocmask(int how, const sigset_t *set,
				    sigset_t *oldset);
extern int monitor_real_pthread_sigmask(int how, const sigset_t *set,
					sigset_t *oldset);
extern int monitor_sigaction(int sig, monitor_sighandler_t *handler,
			     int flags, struct sigaction *act);
extern int monitor_is_threaded(void);
extern void *monitor_get_user_data(void);
extern int monitor_get_thread_num(void);
extern void *monitor_stack_bottom(void);
extern int monitor_in_start_func_wide(void *addr);
extern int monitor_in_start_func_narrow(void *addr);
extern int monitor_unwind_process_bottom_frame(void *addr);
extern int monitor_unwind_thread_bottom_frame(void *addr);
extern int monitor_mpi_comm_size(void);
extern int monitor_mpi_comm_rank(void);
extern void monitor_broadcast_signal (int signum);

#ifdef __cplusplus
}
#endif

#endif  /* ! _MONITOR_H_ */
