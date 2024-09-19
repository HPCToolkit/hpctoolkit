// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Include file for libmonitor clients.
 */

#ifndef  _MONITOR_H_
#define  _MONITOR_H_

#include "pthread_h.h"
#include <sys/types.h>
#include <signal.h>
#include "../foil/libc.h"
#include "../foil/mpi.h"

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

// Foil bases for libmonitor intercepted functions
#ifdef HOST_CPU_PPC
#define START_MAIN_PARAM_LIST           \
    int  argc,                          \
    char **argv,                        \
    char **envp,                        \
    void *auxp,                         \
    void (*rtld_fini)(void),            \
    void **stinfo,                      \
    void *stack_end
#else  /* default __libc_start_main() args */
#define START_MAIN_PARAM_LIST           \
    int  (*main)(int, char **, char **),  \
    int  argc,                          \
    char **argv,                        \
    void (*init)(void),                 \
    void (*fini)(void),                 \
    void (*rtld_fini)(void),            \
    void *stack_end
#endif

extern int hpcrun_libc_start_main(START_MAIN_PARAM_LIST,
                                    const struct hpcrun_foil_appdispatch_libc* dispatch);

extern pid_t hpcrun_fork(const struct hpcrun_foil_appdispatch_libc* dispatch);
extern pid_t hpcrun_vfork(const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_execv(const char *path, char *const argv[],
                          const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_execvp(const char *path, char *const argv[],
                           const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_execve(const char *path, char *const argv[], char *const envp[],
                           const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_system(const char *command, const struct hpcrun_foil_appdispatch_libc* dispatch);
extern void hpcrun_exit(int status, const struct hpcrun_foil_appdispatch_libc* dispatch);
extern void hpcrun_uexit(int status, const struct hpcrun_foil_appdispatch_libc* dispatch);
extern void hpcrun_uExit(int status, const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact,
                              const struct hpcrun_foil_appdispatch_libc* dispatch);
typedef void (*sighandler_t)(int);
extern sighandler_t hpcrun_signal(int sig, sighandler_t handler,
                                    const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_sigprocmask(int how, const sigset_t *set, sigset_t *oldset,
                                const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_sigwait(const sigset_t *set, int *sig,
                            const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_sigwaitinfo(const sigset_t *set, siginfo_t *info,
                                const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_sigtimedwait(const sigset_t *set, siginfo_t *info,
    const struct timespec *timeout, const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    pthread_start_fcn_t *start_routine, void *arg,
    void* caller, const struct hpcrun_foil_appdispatch_libc* dispatch);
extern void hpcrun_pthread_exit(void *data, const struct hpcrun_foil_appdispatch_libc* dispatch);
extern int hpcrun_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset,
                                    const struct hpcrun_foil_appdispatch_libc* dispatch);

extern int hpcrun_MPI_Comm_rank(void *comm, int *rank, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_comm_rank_fortran0(int *comm, int *rank, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_comm_rank_fortran1(int *comm, int *rank, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_comm_rank_fortran2(int *comm, int *rank, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern int hpcrun_MPI_Finalize(const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_finalize_fortran0(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_finalize_fortran1(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_finalize_fortran2(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern int hpcrun_MPI_Init(int *argc, char ***argv, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_init_fortran0(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_init_fortran1(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_init_fortran2(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern int hpcrun_MPI_Init_thread(int *argc, char ***argv, int required, int *provided, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_init_thread_fortran0(int *required, int *provided, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_init_thread_fortran1(int *required, int *provided, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_mpi_init_thread_fortran2(int *required, int *provided, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern int hpcrun_PMPI_Init(int *argc, char ***argv, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_init_fortran0(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_init_fortran1(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_init_fortran2(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern int hpcrun_PMPI_Init_thread(int *argc, char ***argv, int required, int *provided, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_init_thread_fortran0(int *required, int *provided, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_init_thread_fortran1(int *required, int *provided, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_init_thread_fortran2(int *required, int *provided, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern int hpcrun_PMPI_Finalize(const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_finalize_fortran0(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_finalize_fortran1(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_finalize_fortran2(int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern int hpcrun_PMPI_Comm_rank(void *comm, int *rank, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_comm_rank_fortran0(int *comm, int *rank, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_comm_rank_fortran1(int *comm, int *rank, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);
extern void hpcrun_pmpi_comm_rank_fortran2(int *comm, int *rank, int *ierror, const struct hpcrun_foil_appdispatch_mpi*);


#ifdef __cplusplus
}
#endif

#endif  /* ! _MONITOR_H_ */
