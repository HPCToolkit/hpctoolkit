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

typedef int mpi_init_fcn_t(int *, char ***);

static mpi_init_fcn_t  *real_mpi_init = NULL;

int
foilbase_MPI_Init(int *argc, char ***argv)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_init, MPI_Init);
    count = monitor_mpi_init_count(1);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
        monitor_mpi_pre_init();
    }
    ret = (*real_mpi_init)(argc, argv);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
        monitor_init_mpi(argc, argv);
    }
    monitor_mpi_init_count(-1);

    return (ret);
}
