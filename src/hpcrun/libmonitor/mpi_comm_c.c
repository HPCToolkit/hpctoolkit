// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override MPI_Comm_rank in C/C++.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

/*
 * In C, MPI_Comm is not always void *, but that seems to be
 * compatible with most libraries.
 */
int
hpcrun_MPI_Comm_rank(void *comm, int *rank, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int size = -1, ret;

    MONITOR_DEBUG("comm = %p\n", comm);
    f_MPI_Comm_size(comm, &size, dispatch);
    ret = f_MPI_Comm_rank(comm, rank, dispatch);
    monitor_set_mpi_size_rank(size, *rank);

    return (ret);
}
