/*
 *  Libmonitor PMPI functions.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  * Neither the name of Rice University (RICE) nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  This software is provided by RICE and contributors "as is" and any
 *  express or implied warranties, including, but not limited to, the
 *  implied warranties of merchantability and fitness for a particular
 *  purpose are disclaimed. In no event shall RICE or contributors be
 *  liable for any direct, indirect, incidental, special, exemplary, or
 *  consequential damages (including, but not limited to, procurement of
 *  substitute goods or services; loss of use, data, or profits; or
 *  business interruption) however caused and on any theory of liability,
 *  whether in contract, strict liability, or tort (including negligence
 *  or otherwise) arising in any way out of the use of this software, even
 *  if advised of the possibility of such damage.
 *
 *  $Id$
 *
 *  Note: when an MPI application is linked with a profiling library,
 *  this effectively shifts U MPI_Init() to U PMPI_Init(), etc.  As a
 *  result, we also need to override the PMPI functions in the dynamic
 *  case.  In the static case, the wrap option handles the overrides
 *  with or without the profiling library.
 *
 *  Thus, we use this file only in the dynamic case, and so it's safe
 *  to put it all in one file.
 */

#include "config.h"
#include "common.h"
#include "monitor.h"
#include <stdio.h>

typedef int mpi_init_fcn_t(int *, char ***);
typedef int mpi_init_thread_fcn_t(int *, char ***, int, int *);
typedef int mpi_finalize_fcn_t(void);
typedef int mpi_comm_fcn_t(void *, int *);

typedef void f_mpi_init_fcn_t(int *);
typedef void f_mpi_init_thread_fcn_t(int *, int *, int *);
typedef void f_mpi_finalize_fcn_t(int *);
typedef void f_mpi_comm_fcn_t(int *, int *, int *);

static mpi_init_fcn_t    *real_pmpi_init = NULL;
static f_mpi_init_fcn_t  *real_pmpi_init_f0 = NULL;
static f_mpi_init_fcn_t  *real_pmpi_init_f1 = NULL;
static f_mpi_init_fcn_t  *real_pmpi_init_f2 = NULL;

static mpi_init_thread_fcn_t    *real_pmpi_init_thread = NULL;
static f_mpi_init_thread_fcn_t  *real_pmpi_init_thread_f0 = NULL;
static f_mpi_init_thread_fcn_t  *real_pmpi_init_thread_f1 = NULL;
static f_mpi_init_thread_fcn_t  *real_pmpi_init_thread_f2 = NULL;

static mpi_finalize_fcn_t    *real_pmpi_finalize = NULL;
static f_mpi_finalize_fcn_t  *real_pmpi_finalize_f0 = NULL;
static f_mpi_finalize_fcn_t  *real_pmpi_finalize_f1 = NULL;
static f_mpi_finalize_fcn_t  *real_pmpi_finalize_f2 = NULL;

static mpi_comm_fcn_t    *real_pmpi_comm_size = NULL;
static f_mpi_comm_fcn_t  *real_pmpi_comm_size_f0 = NULL;
static f_mpi_comm_fcn_t  *real_pmpi_comm_size_f1 = NULL;
static f_mpi_comm_fcn_t  *real_pmpi_comm_size_f2 = NULL;

static mpi_comm_fcn_t    *real_pmpi_comm_rank = NULL;
static f_mpi_comm_fcn_t  *real_pmpi_comm_rank_f0 = NULL;
static f_mpi_comm_fcn_t  *real_pmpi_comm_rank_f1 = NULL;
static f_mpi_comm_fcn_t  *real_pmpi_comm_rank_f2 = NULL;

/*
 *----------------------------------------------------------------------
 *  PMPI_INIT OVERRIDE FUNCTIONS
 *----------------------------------------------------------------------
 */

int
MONITOR_WRAP_NAME(PMPI_Init)(int *argc, char ***argv)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_pmpi_init, PMPI_Init);
    count = monitor_mpi_init_count(1);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
	monitor_mpi_pre_init();
    }
    ret = (*real_pmpi_init)(argc, argv);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
	monitor_init_mpi(argc, argv);
    }
    monitor_mpi_init_count(-1);

    return (ret);
}

#define FORTRAN_INIT_BODY(var_name, fcn_name)			\
    int argc, count;						\
    char **argv;						\
    MONITOR_DEBUG1("\n");					\
    MONITOR_GET_REAL_NAME_WRAP(var_name, fcn_name);		\
    count = monitor_mpi_init_count(1);				\
    if (count == 1) {						\
	MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n"); \
	monitor_mpi_pre_init();					\
    }								\
    (*var_name)(ierror);					\
    if (count == 1) {						\
	MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");	\
	monitor_get_main_args(&argc, &argv, NULL);		\
	monitor_init_mpi(&argc, &argv);				\
    }								\
    monitor_mpi_init_count(-1);

void
MONITOR_WRAP_NAME(pmpi_init)(int *ierror)
{
    FORTRAN_INIT_BODY(real_pmpi_init_f0, pmpi_init);
}

void
MONITOR_WRAP_NAME(pmpi_init_)(int *ierror)
{
    FORTRAN_INIT_BODY(real_pmpi_init_f1, pmpi_init_);
}

void
MONITOR_WRAP_NAME(pmpi_init__)(int *ierror)
{
    FORTRAN_INIT_BODY(real_pmpi_init_f2, pmpi_init__);
}

/*
 *----------------------------------------------------------------------
 *  PMPI_INIT_THREAD OVERRIDE FUNCTIONS
 *----------------------------------------------------------------------
 */

int
MONITOR_WRAP_NAME(PMPI_Init_thread)(int *argc, char ***argv,
				    int required, int *provided)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_pmpi_init_thread, PMPI_Init_thread);
    count = monitor_mpi_init_count(1);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");
	monitor_mpi_pre_init();
    }
    ret = (*real_pmpi_init_thread)(argc, argv, required, provided);
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");
	monitor_init_mpi(argc, argv);
    }
    monitor_mpi_init_count(-1);

    return (ret);
}

#define FORTRAN_INIT_THREAD_BODY(var_name, fcn_name)		\
    int argc, count;						\
    char **argv;						\
    MONITOR_DEBUG1("\n");					\
    MONITOR_GET_REAL_NAME_WRAP(var_name, fcn_name);		\
    count = monitor_mpi_init_count(1);				\
    if (count == 1) {						\
	MONITOR_DEBUG1("calling monitor_mpi_pre_init() ...\n");	\
	monitor_mpi_pre_init();					\
    }								\
    (*var_name)(required, provided, ierror);			\
    if (count == 1) {						\
	MONITOR_DEBUG1("calling monitor_init_mpi() ...\n");	\
	monitor_get_main_args(&argc, &argv, NULL);		\
	monitor_init_mpi(&argc, &argv);				\
    }								\
    monitor_mpi_init_count(-1);

void
MONITOR_WRAP_NAME(pmpi_init_thread)(int *required, int *provided, int *ierror)
{
    FORTRAN_INIT_THREAD_BODY(real_pmpi_init_thread_f0, pmpi_init_thread);
}

void
MONITOR_WRAP_NAME(pmpi_init_thread_)(int *required, int *provided, int *ierror)
{
    FORTRAN_INIT_THREAD_BODY(real_pmpi_init_thread_f1, pmpi_init_thread_);
}

void
MONITOR_WRAP_NAME(pmpi_init_thread__)(int *required, int *provided, int *ierror)
{
    FORTRAN_INIT_THREAD_BODY(real_pmpi_init_thread_f2, pmpi_init_thread__);
}

/*
 *----------------------------------------------------------------------
 *  PMPI_FINALIZE OVERRIDE FUNCTIONS
 *----------------------------------------------------------------------
 */

int
MONITOR_WRAP_NAME(PMPI_Finalize)(void)
{
    int ret, count;

    MONITOR_DEBUG1("\n");
    MONITOR_GET_REAL_NAME_WRAP(real_pmpi_finalize, PMPI_Finalize);
    count = monitor_mpi_fini_count(1);
    if (count == 1) {
	MONITOR_DEBUG("calling monitor_fini_mpi(), size = %d, rank = %d ...\n",
		      monitor_mpi_comm_size(), monitor_mpi_comm_rank());
	monitor_fini_mpi();
    }
    ret = (*real_pmpi_finalize)();
    if (count == 1) {
	MONITOR_DEBUG1("calling monitor_mpi_post_fini() ...\n");
	monitor_mpi_post_fini();
    }
    monitor_mpi_fini_count(-1);

    return (ret);
}

#define FORTRAN_FINALIZE_BODY(var_name, fcn_name)	\
    int count;						\
    MONITOR_DEBUG1("\n");				\
    MONITOR_GET_REAL_NAME_WRAP(var_name, fcn_name);	\
    count = monitor_mpi_fini_count(1);			\
    if (count == 1) {					\
	MONITOR_DEBUG("calling monitor_fini_mpi(), size = %d, rank = %d ...\n",  \
		      monitor_mpi_comm_size(), monitor_mpi_comm_rank());  \
	monitor_fini_mpi();				\
    }							\
    (*var_name)(ierror);				\
    if (count == 1) {					\
	MONITOR_DEBUG1("calling monitor_mpi_post_fini() ...\n");  \
	monitor_mpi_post_fini();			\
    }							\
    monitor_mpi_fini_count(-1);

void
MONITOR_WRAP_NAME(pmpi_finalize)(int *ierror)
{
    FORTRAN_FINALIZE_BODY(real_pmpi_finalize_f0, pmpi_finalize);
}

void
MONITOR_WRAP_NAME(pmpi_finalize_)(int *ierror)
{
    FORTRAN_FINALIZE_BODY(real_pmpi_finalize_f1, pmpi_finalize_);
}

void
MONITOR_WRAP_NAME(pmpi_finalize__)(int *ierror)
{
    FORTRAN_FINALIZE_BODY(real_pmpi_finalize_f2, pmpi_finalize__);
}

/*
 *----------------------------------------------------------------------
 *  OVERRIDE PMPI_COMM_RANK FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 * In C, MPI_Comm is not always void *, but that seems to be
 * compatible with most libraries.
 */
int
MONITOR_WRAP_NAME(PMPI_Comm_rank)(void *comm, int *rank)
{
    int size = -1, ret;

    MONITOR_DEBUG("comm = %p\n", comm);
    MONITOR_GET_REAL_NAME(real_pmpi_comm_size, PMPI_Comm_size);
    MONITOR_GET_REAL_NAME_WRAP(real_pmpi_comm_rank, PMPI_Comm_rank);
    ret = (*real_pmpi_comm_size)(comm, &size);
    ret = (*real_pmpi_comm_rank)(comm, rank);
    monitor_set_mpi_size_rank(size, *rank);

    return (ret);
}

#define FORTRAN_COMM_RANK_BODY(size_var, size_fcn, rank_var, rank_fcn)  \
    int size = -1;					\
    MONITOR_DEBUG("comm = %d\n", *comm);		\
    MONITOR_GET_REAL_NAME(size_var, size_fcn);		\
    MONITOR_GET_REAL_NAME_WRAP(rank_var, rank_fcn);	\
    (*size_var)(comm, &size, ierror);			\
    (*rank_var)(comm, rank, ierror);			\
    monitor_set_mpi_size_rank(size, *rank);

/*
 * In Fortran, MPI_Comm is always int.
 */
void
MONITOR_WRAP_NAME(pmpi_comm_rank)(int *comm, int *rank, int *ierror)
{
    FORTRAN_COMM_RANK_BODY(real_pmpi_comm_size_f0, pmpi_comm_size,
			   real_pmpi_comm_rank_f0, pmpi_comm_rank);
}

void
MONITOR_WRAP_NAME(pmpi_comm_rank_)(int *comm, int *rank, int *ierror)
{
    FORTRAN_COMM_RANK_BODY(real_pmpi_comm_size_f1, pmpi_comm_size_,
			   real_pmpi_comm_rank_f1, pmpi_comm_rank_);
}

void
MONITOR_WRAP_NAME(pmpi_comm_rank__)(int *comm, int *rank, int *ierror)
{
    FORTRAN_COMM_RANK_BODY(real_pmpi_comm_size_f2, pmpi_comm_size__,
			   real_pmpi_comm_rank_f2, pmpi_comm_rank__);
}
