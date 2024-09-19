// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override MPI_Finalize in C/C++.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

int
hpcrun_MPI_Finalize(const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    count = monitor_mpi_fini_count(1);
    if (count == 1) {
        MONITOR_DEBUG("calling monitor_fini_mpi(), size = %d, rank = %d ...\n",
                      monitor_mpi_comm_size(), monitor_mpi_comm_rank());
        monitor_fini_mpi();
    }
    ret = f_MPI_Finalize(dispatch);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_post_fini() ...\n");
        monitor_mpi_post_fini();
    }
    monitor_mpi_fini_count(-1);

    return (ret);
}
