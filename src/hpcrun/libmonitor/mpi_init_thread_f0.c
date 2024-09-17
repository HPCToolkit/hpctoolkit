// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override mpi_init_thread in Fortran.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

void
hpcrun_mpi_init_thread_fortran0(int *required, int *provided, int *ierror,
                         const struct hpcrun_foil_appdispatch_mpi* dispatch)
{
    int argc, count;
    char **argv;

    MONITOR_DEBUG1("\n");
    count = monitor_mpi_init_count(1);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
        monitor_mpi_pre_init();
    }
    f_mpi_init_thread_fortran0(required, provided, ierror, dispatch);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
        monitor_get_main_args(&argc, &argv, NULL);
        monitor_init_mpi(&argc, &argv);
    }
    monitor_mpi_init_count(-1);
}
