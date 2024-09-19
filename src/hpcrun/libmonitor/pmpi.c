// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor PMPI functions.
 *
 *  Note: when an MPI application is linked with a profiling library,
 *  this effectively shifts U MPI_Init() to U PMPI_Init(), etc.  As a
 *  result, we also need to override the PMPI functions in the dynamic
 *  case.  In the static case, the wrap option handles the overrides
 *  with or without the profiling library.
 *
 *  Thus, we use this file only in the dynamic case, and so it's safe
 *  to put it all in one file.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"
#include <stdio.h>

/*
 *----------------------------------------------------------------------
 *  PMPI_INIT OVERRIDE FUNCTIONS
 *----------------------------------------------------------------------
 */

int
hpcrun_PMPI_Init(int *argc, char ***argv, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    count = monitor_mpi_init_count(1);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
        monitor_mpi_pre_init();
    }
    ret = f_PMPI_Init(argc, argv, dispatch);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
        monitor_init_mpi(argc, argv);
    }
    monitor_mpi_init_count(-1);

    return (ret);
}

#define FORTRAN_INIT_BODY(fcn_name)                             \
    int argc, count;                                            \
    char **argv;                                                \
    MONITOR_DEBUG1("\n");                                       \
    count = monitor_mpi_init_count(1);                          \
    if (count == 1) {                                           \
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n"); \
        monitor_mpi_pre_init();                                 \
    }                                                           \
    f_##fcn_name(ierror, dispatch);                             \
    if (count == 1) {                                           \
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");     \
        monitor_get_main_args(&argc, &argv, NULL);              \
        monitor_init_mpi(&argc, &argv);                         \
    }                                                           \
    monitor_mpi_init_count(-1);

void
hpcrun_pmpi_init_fortran0(int *ierror, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_INIT_BODY(pmpi_init_fortran0);
}

void
hpcrun_pmpi_init_fortran1(int *ierror, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_INIT_BODY(pmpi_init_fortran1);
}

void
hpcrun_pmpi_init_fortran2(int *ierror, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_INIT_BODY(pmpi_init_fortran2);
}

/*
 *----------------------------------------------------------------------
 *  PMPI_INIT_THREAD OVERRIDE FUNCTIONS
 *----------------------------------------------------------------------
 */

int
hpcrun_PMPI_Init_thread(int *argc, char ***argv,
    int required, int *provided, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    count = monitor_mpi_init_count(1);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
        monitor_mpi_pre_init();
    }
    ret = f_PMPI_Init_thread(argc, argv, required, provided, dispatch);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
        monitor_init_mpi(argc, argv);
    }
    monitor_mpi_init_count(-1);

    return (ret);
}

#define FORTRAN_INIT_THREAD_BODY(fcn_name)                      \
    int argc, count;                                            \
    char **argv;                                                \
    MONITOR_DEBUG1("\n");                                       \
    count = monitor_mpi_init_count(1);                          \
    if (count == 1) {                                           \
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n"); \
        monitor_mpi_pre_init();                                 \
    }                                                           \
    f_##fcn_name(required, provided, ierror, dispatch);         \
    if (count == 1) {                                           \
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");     \
        monitor_get_main_args(&argc, &argv, NULL);              \
        monitor_init_mpi(&argc, &argv);                         \
    }                                                           \
    monitor_mpi_init_count(-1);

void
hpcrun_pmpi_init_thread_fortran0(int *required, int *provided, int *ierror,
                          const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_INIT_THREAD_BODY(pmpi_init_thread_fortran0);
}

void
hpcrun_pmpi_init_thread_fortran1(int *required, int *provided, int *ierror,
                           const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_INIT_THREAD_BODY(pmpi_init_thread_fortran1);
}

void
hpcrun_pmpi_init_thread_fortran2(int *required, int *provided, int *ierror,
                            const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_INIT_THREAD_BODY(pmpi_init_thread_fortran2);
}

/*
 *----------------------------------------------------------------------
 *  PMPI_FINALIZE OVERRIDE FUNCTIONS
 *----------------------------------------------------------------------
 */

int
hpcrun_PMPI_Finalize(const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    count = monitor_mpi_fini_count(1);
    if (count == 1) {
        MONITOR_DEBUG("calling monitor_fini_mpi(), size = %d, rank = %d ...\n",
                      monitor_mpi_comm_size(), monitor_mpi_comm_rank());
        monitor_fini_mpi();
    }
    ret = f_PMPI_Finalize(dispatch);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_post_fini() ...\n");
        monitor_mpi_post_fini();
    }
    monitor_mpi_fini_count(-1);

    return (ret);
}

#define FORTRAN_FINALIZE_BODY(fcn_name)                 \
    int count;                                          \
    MONITOR_DEBUG1("\n");                               \
    count = monitor_mpi_fini_count(1);                  \
    if (count == 1) {                                   \
        MONITOR_DEBUG("calling monitor_fini_mpi(), size = %d, rank = %d ...\n",  \
                      monitor_mpi_comm_size(), monitor_mpi_comm_rank());  \
        monitor_fini_mpi();                             \
    }                                                   \
    f_##fcn_name(ierror, dispatch);                     \
    if (count == 1) {                                   \
        MONITOR_DEBUG1("calling monitor_mpi_post_fini() ...\n");  \
        monitor_mpi_post_fini();                        \
    }                                                   \
    monitor_mpi_fini_count(-1);

void
hpcrun_pmpi_finalize_fortran0(int *ierror, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_FINALIZE_BODY(pmpi_finalize_fortran0);
}

void
hpcrun_pmpi_finalize_fortran1(int *ierror, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_FINALIZE_BODY(pmpi_finalize_fortran1);
}

void
hpcrun_pmpi_finalize_fortran2(int *ierror, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_FINALIZE_BODY(pmpi_finalize_fortran2);
}

/*
 *----------------------------------------------------------------------
 *  OVERRIDE PMPI_COMM_RANK FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 * In C, MPI_Comm is not always void *, but that seems to be
 * compatible with most libraries.
 */
int
hpcrun_PMPI_Comm_rank(void *comm, int *rank, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int size = -1, ret;

    MONITOR_DEBUG("comm = %p\n", comm);
    ret = f_PMPI_Comm_size(comm, &size, dispatch);
    ret = f_PMPI_Comm_rank(comm, rank, dispatch);
    monitor_set_mpi_size_rank(size, *rank);

    return (ret);
}

#define FORTRAN_COMM_RANK_BODY(size_fcn, rank_fcn)      \
    int size = -1;                                      \
    MONITOR_DEBUG("comm = %d\n", *comm);                \
    f_##size_fcn(comm, &size, ierror, dispatch);        \
    f_##rank_fcn(comm, rank, ierror, dispatch);         \
    monitor_set_mpi_size_rank(size, *rank);

/*
 * In Fortran, MPI_Comm is always int.
 */
void
hpcrun_pmpi_comm_rank_fortran0(int *comm, int *rank, int *ierror,
                        const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_COMM_RANK_BODY(pmpi_comm_size_fortran0, pmpi_comm_rank_fortran0);
}

void
hpcrun_pmpi_comm_rank_fortran1(int *comm, int *rank, int *ierror,
                         const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_COMM_RANK_BODY(pmpi_comm_size_fortran1, pmpi_comm_rank_fortran1);
}

void
hpcrun_pmpi_comm_rank_fortran2(int *comm, int *rank, int *ierror,
                          const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    FORTRAN_COMM_RANK_BODY(pmpi_comm_size_fortran2, pmpi_comm_rank_fortran2);
}
