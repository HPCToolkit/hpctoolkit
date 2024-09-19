// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override MPI_Init in C/C++.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

int
hpcrun_MPI_Init(int *argc, char ***argv, const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    count = monitor_mpi_init_count(1);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
        monitor_mpi_pre_init();
    }
    ret = f_MPI_Init(argc, argv, dispatch);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
        monitor_init_mpi(argc, argv);
    }
    monitor_mpi_init_count(-1);

    return (ret);
}
