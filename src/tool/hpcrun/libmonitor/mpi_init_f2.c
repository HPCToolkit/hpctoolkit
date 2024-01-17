/*
 *  Override mpi_init__ in Fortran.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  See the file LICENSE for details.
 *
 *  $Id$
 */

#include "config.h"
#include "common.h"
#include "monitor.h"

typedef void mpi_init_fcn_t(int *);

#ifdef MONITOR_STATIC
extern mpi_init_fcn_t  __real_mpi_init__;
#endif
static mpi_init_fcn_t  *real_mpi_init = NULL;

void
MONITOR_WRAP_NAME(mpi_init__)(int *ierror)
{
    int argc, count;
    char **argv;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_init, mpi_init__);
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
