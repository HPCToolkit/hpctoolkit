/*
 *  Override MPI_Init_thread in C/C++.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  See the file LICENSE for details.
 *
 *  $Id$
 */

#include "config.h"
#include "common.h"
#include "monitor.h"

typedef int mpi_init_thread_fcn_t(int *, char ***, int, int *);

#ifdef MONITOR_STATIC
extern mpi_init_thread_fcn_t  __real_MPI_Init_thread;
#endif
static mpi_init_thread_fcn_t  *real_mpi_init_thread = NULL;

int
MONITOR_WRAP_NAME(MPI_Init_thread)(int *argc, char ***argv,
				   int required, int *provided)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_mpi_init_thread, MPI_Init_thread);
    count = monitor_mpi_init_count(1);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
	monitor_mpi_pre_init();
    }
    ret = (*real_mpi_init_thread)(argc, argv, required, provided);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
	monitor_init_mpi(argc, argv);
    }
    monitor_mpi_init_count(-1);

    return (ret);
}
