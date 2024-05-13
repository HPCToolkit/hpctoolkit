// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor common MPI functions.
 *
 *  Note: it's ok to always include this file in the build, even when
 *  MPI is disabled, as long as it contains no override functions and
 *  does not call any real MPI functions.  This eliminates the need
 *  for defining weak versions of these functions in main.c.
 */

#define _GNU_SOURCE

#include "common.h"
#include "monitor.h"

static int mpi_init_count = 0;
static int mpi_fini_count = 0;
static int max_init_count = 0;
static int mpi_size = -1;
static int mpi_rank = -1;

/*
 *----------------------------------------------------------------------
 *  CLIENT SUPPORT FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Some Fortran implementations call mpi_init_() which then calls
 *  MPI_Init().  So, we count the depth of nesting in order to call
 *  the callback function just once per application call.
 */
int
monitor_mpi_init_count(int inc)
{
    mpi_init_count += inc;
    if (mpi_init_count > max_init_count)
        max_init_count = mpi_init_count;
    return (mpi_init_count);
}

int
monitor_mpi_fini_count(int inc)
{
    mpi_fini_count += inc;
    return (mpi_fini_count);
}

/*
 *  Allow the client to publish its own size and rank.  This is useful
 *  for parallel applications that are not based on MPI, eg: Gasnet
 *  and Rice Coarray Fortran.  If called more than once, the first one
 *  wins (ensures consistent answers).
 *
 *  FIXME: this interface may be subject to change (it shouldn't be
 *  mixed with MPI).
 */
void
monitor_set_size_rank(int size, int rank)
{
    static int first = 1;

    if (first) {
        MONITOR_DEBUG("setting size = %d, rank = %d\n", size, rank);
        mpi_size = size;
        mpi_rank = rank;
        first = 0;
    }
}

/*
 *  Set in the C or Fortran MPI_Comm_rank() override function.
 *
 *  Note: we depend on the application using MPI_COMM_WORLD before any
 *  other communicator, so we only want to set size/rank on the first
 *  use of MPI_Comm_rank().  But wait until after MPI_Init() finishes
 *  in case it calls MPI_Comm_rank() itself with some other
 *  communicator (some libraries do this).
 */
void
monitor_set_mpi_size_rank(int size, int rank)
{
    static int first = 1;

    if (first && mpi_init_count == 0 && max_init_count > 0) {
        monitor_set_size_rank(size, rank);
        first = 0;
    }
}

int
monitor_mpi_comm_size(void)
{
    return (mpi_size);
}

int
monitor_mpi_comm_rank(void)
{
    return (mpi_rank);
}
