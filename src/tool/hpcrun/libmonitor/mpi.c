/*
 *  Libmonitor common MPI functions.
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
 *  Note: it's ok to always include this file in the build, even when
 *  MPI is disabled, as long as it contains no override functions and
 *  does not call any real MPI functions.  This eliminates the need
 *  for defining weak versions of these functions in main.c.
 */

#include "config.h"
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
