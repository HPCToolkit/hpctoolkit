// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override mpi_comm_rank in Fortran.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

/*
 * In Fortran, MPI_Comm is always int.
 */
void
hpcrun_mpi_comm_rank_fortran0(int *comm, int *rank, int *ierror, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int size = -1;

    MONITOR_DEBUG("comm = %d\n", *comm);
    f_mpi_comm_size_fortran0(comm, &size, ierror, dispatch);
    f_mpi_comm_rank_fortran0(comm, rank, ierror, dispatch);
    monitor_set_mpi_size_rank(size, *rank);
}
