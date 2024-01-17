/*
 *  Include file for libmonitor clients.
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

#ifndef  _MONITOR_H_
#define  _MONITOR_H_

#include <sys/types.h>
#include <signal.h>

typedef int monitor_sighandler_t(int, siginfo_t *, void *);

enum { MONITOR_EXIT_NORMAL = 1, MONITOR_EXIT_SIGNAL, MONITOR_EXIT_EXEC };

#define MONITOR_IGNORE_NEW_THREAD  ((void *) -1)

#ifdef __cplusplus
extern "C" {
#endif

struct monitor_thread_info {
    void * mti_create_return_addr;
    void * mti_start_routine;
};

/*
 *  Callback functions for the client to override.
 */
extern void monitor_init_library(void);
extern void monitor_fini_library(void);
extern void *monitor_init_process(int *argc, char **argv, void *data);
extern void monitor_fini_process(int how, void *data);
extern void monitor_start_main_init(void);
extern void monitor_at_main(void);
extern void monitor_begin_process_exit(int);
extern void *monitor_pre_fork(void);
extern void monitor_post_fork(pid_t child, void *data);
extern void *monitor_thread_pre_create(void);
extern void monitor_thread_post_create(void *);
extern void monitor_init_thread_support(void);
extern void *monitor_init_thread(int tid, void *data);
extern void monitor_fini_thread(void *data);
extern size_t monitor_reset_stacksize(size_t old_size);
extern void monitor_pre_dlopen(const char *path, int flags);
extern void monitor_dlopen(const char *path, int flags, void *handle);
extern void monitor_dlclose(void *handle);
extern void monitor_post_dlclose(void *handle, int ret);
extern void monitor_mpi_pre_init(void);
extern void monitor_init_mpi(int *argc, char ***argv);
extern void monitor_fini_mpi(void);
extern void monitor_mpi_post_fini(void);

/*
 *  Monitor support functions.
 */
extern void monitor_initialize(void);
extern void monitor_real_abort(void);
extern void monitor_real_exit(int status);
extern pid_t monitor_real_fork(void);
extern int monitor_real_execve(const char *path, char *const argv[],
			       char *const envp[]);
extern int monitor_real_system(const char *command);
extern void *monitor_real_dlopen(const char *path, int flags);
extern int monitor_real_dlclose(void *handle);
extern int monitor_real_sigprocmask(int how, const sigset_t *set,
				    sigset_t *oldset);
extern int monitor_real_pthread_sigmask(int how, const sigset_t *set,
					sigset_t *oldset);
extern int monitor_sigaction(int sig, monitor_sighandler_t *handler,
			     int flags, struct sigaction *act);
extern int monitor_broadcast_signal(int sig);
extern int monitor_is_threaded(void);
extern void *monitor_get_addr_main(void);
extern void *monitor_get_addr_thread_start(void);
extern void *monitor_get_user_data(void);
extern int monitor_get_thread_num(void);
extern void *monitor_stack_bottom(void);
extern int monitor_in_start_func_wide(void *addr);
extern int monitor_in_start_func_narrow(void *addr);
extern int monitor_unwind_process_bottom_frame(void *addr);
extern int monitor_unwind_thread_bottom_frame(void *addr);
extern void monitor_set_size_rank(int, int);
extern int monitor_mpi_comm_size(void);
extern int monitor_mpi_comm_rank(void);
extern int monitor_block_shootdown(void);
extern void monitor_unblock_shootdown(void);
extern void monitor_disable_new_threads(void);
extern void monitor_enable_new_threads(void);
extern int monitor_get_new_thread_info(struct monitor_thread_info *);

/*
 *  Special access to wrapped functions for the application.
 */
extern int monitor_wrap_main(int, char **, char **);

#ifdef __cplusplus
}
#endif

#endif  /* ! _MONITOR_H_ */
