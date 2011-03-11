// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: $
// $Id: $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *



/******************************************************************************
 * standard include files
 *****************************************************************************/

#include <assert.h>
#include <ucontext.h>

#include <stdlib.h>

/* definition for valloc, memalign */
#include <malloc.h>

/* definition for sysconf */
#include <unistd.h>

/* definition for _SC_PAGESIZE */
#include <sys/mman.h>

#include <mpi.h>

/******************************************************************************
 * local include files
 *****************************************************************************/

#include <messages/messages.h>
#include <sample_event.h>
#include <monitor-exts/monitor_ext.h>




/******************************************************************************
 * type definitions
 *****************************************************************************/




/******************************************************************************
 * macros
 *****************************************************************************/

#define HPCRUN_MPI_WRAP MONITOR_EXT_WRAP_NAME

/******************************************************************************
 * externals functions
 *****************************************************************************/

extern int hpcrun_mpi_metric_id();


/******************************************************************************
 * private variables
 *****************************************************************************/




/******************************************************************************
 * private operations
 *****************************************************************************/


/** Convert a (count/datatype) into a size in bytes
 * Note:
 * Eventually we may want to have a short cut for this for
 * know implementation.  E.g., in MPICH2, the size may
 * be extracted from basic datatypes without a routine call.
 */
static inline int Get_Msg_size( int count, MPI_Datatype datatype )
{
    int dsize;

    PMPI_Type_size( datatype, &dsize );
    return count * dsize;
}



static void hpmpi_store_metric(size_t bytes)
{
    ucontext_t uc;
    getcontext(&uc);

    hpcrun_async_block();

    cct_node_t * cct = hpcrun_sample_callpath(&uc, hpcrun_mpi_metric_id(), bytes, 0, 1);

    TMSG(MPI, "cct: %p, bytes: %d", cct, bytes );

    hpcrun_async_unblock();

}


/******************************************************************************
 * interface operations
 *****************************************************************************/


int
HPCRUN_MPI_WRAP(MPI_Allgather)(  void *sendbuf, int sendcount, MPI_Datatype sendtype,
        void *recvbuf, int recvcount, MPI_Datatype recvtype,
        MPI_Comm comm  )
{

  int bytes = Get_Msg_size(sendcount, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Allgather( sendbuf, sendcount, sendtype, recvbuf,
          recvcount, recvtype, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Allgatherv)(  void *sendbuf, int sendcount, MPI_Datatype sendtype,
        void *recvbuf, int *recvcount, int *displs, MPI_Datatype recvtype,
        MPI_Comm comm  )
{

  int bytes = Get_Msg_size(sendcount, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Allgatherv( sendbuf, sendcount, sendtype, recvbuf,
          recvcount, displs, recvtype, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Allreduce)( void *sendbuf, void *recvbuf, int count,
        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm )
{

  int bytes = Get_Msg_size(count, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Allreduce( sendbuf, recvbuf, count, datatype, op, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Alltoall)( void *sendbuf, int sendcount, MPI_Datatype sendtype,
		  void *recvbuf, int recvcnt, MPI_Datatype recvtype,
		  MPI_Comm comm )
{

  int bytes = Get_Msg_size(sendcount, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Alltoall( sendbuf, sendcount, sendtype, recvbuf,
		     recvcnt, recvtype, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Alltoallv)( void *sendbuf, int *sendcnts, int *sdispls,
		   MPI_Datatype sendtype, void *recvbuf, int *recvcnts,
		   int *rdispls, MPI_Datatype recvtype, MPI_Comm comm )
{

  int bytes = Get_Msg_size( *sendcnts, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Alltoallv( sendbuf, sendcnts, sdispls, sendtype, recvbuf,
	      recvcnts, rdispls, recvtype, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Bcast)( void *buffer, int count, MPI_Datatype datatype,
	       int root, MPI_Comm comm )
{

  int bytes = Get_Msg_size(count, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Bcast( buffer, count, datatype, root, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Gather)(void *sendbuf, int sendcnt, MPI_Datatype sendtype,
		void *recvbuf, int recvcount, MPI_Datatype recvtype,
		int root, MPI_Comm comm )
{

  int bytes = Get_Msg_size(sendcnt, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Gather( sendbuf, sendcnt, sendtype, recvbuf,
		   recvcount, recvtype, root, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Gatherv)(void *sendbuf, int sendcnt, MPI_Datatype sendtype,
		 void *recvbuf, int *recvcnts, int *displs,
		 MPI_Datatype recvtype, int root, MPI_Comm comm )
{

  int bytes = Get_Msg_size(sendcnt, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Gatherv( sendbuf, sendcnt, sendtype, recvbuf, recvcnts,
		    displs, recvtype, root, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Reduce_scatter)( void *sendbuf, void *recvbuf, int *recvcnts,
		MPI_Datatype datatype, MPI_Op op, MPI_Comm comm )
{

  int bytes = Get_Msg_size(*recvcnts, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Reduce_scatter( sendbuf, recvbuf, recvcnts, datatype,
		   op, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Reduce)( void *sendbuf, void *recvbuf, int count,
		  MPI_Datatype datatype, MPI_Op op, int root,
		  MPI_Comm comm )
{

  int bytes = Get_Msg_size(count, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Reduce( sendbuf, recvbuf, count, datatype, op, root, comm );

}


int
HPCRUN_MPI_WRAP(MPI_Scan)( void *sendbuf, void *recvbuf, int count,
		MPI_Datatype datatype, MPI_Op op, MPI_Comm comm )
{

  int bytes = Get_Msg_size(count, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Scan( sendbuf, recvbuf, count, datatype, op, comm );

}

int
HPCRUN_MPI_WRAP(MPI_Send)(  void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm )
{

  int bytes = Get_Msg_size(count, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Send( buf, count, datatype, dest, tag, comm);

}


int
HPCRUN_MPI_WRAP(MPI_Recv)(  void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm, MPI_Status *status )
{
  // call the real mpi first
  int return_val = PMPI_Recv( buf, count, datatype, dest, tag, comm, status);

  MPI_Status *sp = status;
  int cnt = count; 	// temporary variable to store the number of items
    				// remark: the number of items received can be different to "count" !!
  PMPI_Get_count( sp, datatype, &cnt );

  int bytes = Get_Msg_size(cnt, datatype );

  hpmpi_store_metric(bytes);

  return return_val;

}

