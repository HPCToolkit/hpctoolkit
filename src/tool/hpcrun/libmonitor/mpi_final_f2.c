/*
 *  Override mpi_finalize__ in Fortran.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  See the file LICENSE for details.
 *
 *  $Id$
 */

#include "common.h"
#include "monitor.h"

typedef void mpi_finalize_fcn_t(int *);

static mpi_finalize_fcn_t  *real_mpi_finalize = NULL;

void
foilbase_mpi_finalize__(int *ierror)
{
    int count;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_finalize, mpi_finalize__);
    count = monitor_mpi_fini_count(1);
    if (count == 1) {
        MONITOR_DEBUG("calling monitor_fini_mpi(), size = %d, rank = %d ...\n",
                      monitor_mpi_comm_size(), monitor_mpi_comm_rank());
        monitor_fini_mpi();
    }
    (*real_mpi_finalize)(ierror);
    if (count == 1) {
        MONITOR_DEBUG1("calling monitor_mpi_post_fini() ...\n");
        monitor_mpi_post_fini();
    }
    monitor_mpi_fini_count(-1);
}