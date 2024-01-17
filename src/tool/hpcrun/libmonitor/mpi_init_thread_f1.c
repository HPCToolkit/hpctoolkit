/*
 *  Override mpi_init_thread_ in Fortran.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  See the file LICENSE for details.
 *
 *  $Id$
 */

#include "config.h"
#include "common.h"
#include "monitor.h"

typedef void mpi_init_thread_fcn_t(int *, int *, int *);

#ifdef MONITOR_STATIC
extern mpi_init_thread_fcn_t  __real_mpi_init_thread_;
#endif
static mpi_init_thread_fcn_t  *real_mpi_init_thread = NULL;

void
MONITOR_WRAP_NAME(mpi_init_thread_)(int *required, int *provided, int *ierror)
{
    int argc, count;
    char **argv;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_init_thread, mpi_init_thread_);
    count = monitor_mpi_init_count(1);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
	monitor_mpi_pre_init();
    }
    (*real_mpi_init_thread)(required, provided, ierror);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
	monitor_get_main_args(&argc, &argv, NULL);
	monitor_init_mpi(&argc, &argv);
    }
    monitor_mpi_init_count(-1);
}
