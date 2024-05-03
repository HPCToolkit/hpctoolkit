// SPDX-FileCopyrightText: 2007-2023 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Override mpi_init_ in Fortran.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

typedef void mpi_init_fcn_t(int *);

static mpi_init_fcn_t  *real_mpi_init = NULL;

void
foilbase_mpi_init_(int *ierror)
{
    int argc, count;
    char **argv;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_init, mpi_init_);
    count = monitor_mpi_init_count(1);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
        monitor_mpi_pre_init();
    }
    (*real_mpi_init)(ierror);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
        monitor_get_main_args(&argc, &argv, NULL);
        monitor_init_mpi(&argc, &argv);
    }
    monitor_mpi_init_count(-1);
}
