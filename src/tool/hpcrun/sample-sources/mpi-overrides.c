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


enum bin_sizes {sz_0, sz_1to4, sz_5to8, sz_9to16, sz_17to32, sz_33to64,
		sz_65to128, sz_129to256, sz_257to512, sz_513to1024,
		sz_1to4K, sz_5to8K, sz_9to16K, sz_17to32K, sz_33to64K,
		sz_65to128K, sz_129to256K, sz_257to512K, sz_513to1024K,
		sz_1to4M, sz_5to8M, sz_9to16M, sz_17to32M, sz_33to64M,
		sz_65to128M, sz_129to256M, sz_257to512M, sz_513to1024M,
		sz_large};

/******************************************************************************
 * macros
 *****************************************************************************/

#define ONE_MB 1048576
#define ONE_KB 1024
#define MAX_MSG 1073741824
#define NBINS 32


/******************************************************************************
 * externals functions
 *****************************************************************************/

extern int hpcrun_mpi_metric_id();


/******************************************************************************
 * private variables
 *****************************************************************************/

static int hpmpi_metric_id = 0;




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

/* ------------------------------------------------------------------------
 * Service routines for collecting data
 * ------------------------------------------------------------------------
 */
/* Determine message bin ID.
   To keep this fast, the bin id's are a power of two; we can compute the
   bin id with a quick tree code.  STILL TO DO!  FIXME

   We should really just look for 4 * 2^k (since most scientific data
   will be at least a multiple of the size of an int).

   size >>= 2;   // divide by four
   Find the location of the largest bit.
   See http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
   for an example.  An easy code is

   int b = buf_size;
   while (b >>=1) bin_id ++;
   if (bin_id > 1) bin_id -= 1;  // combine 1 and 2,3 byte messages

   This gives:
   bin_id == 0  Messages of 0 bytes
   bin_id == 1  Messages of 1,2,3 bytes
   bin_id == 2  Messages of 4-7 bytes
   bin_id == k  Messages of 2^k - 2^k-1 bytes

   FIXME:
   We should have a default that quickly looks at the high
   bit; an option could allow the selection of user-defined sizes.
   E.g., use
    if (buf_size & 0xffff0000) { bin_id += 16; buf_size &= 0xffff0000; }
    if (buf_size & 0xff00ff00) { bin_id += 8;  buf_size &= 0xff00ff00; }
    if (buf_size & 0xf0f0f0f0) { bin_id += 4;  buf_size &= 0xf0f0f0f0; }
    if (buf_size & 0xcccccccc) { bin_id += 2;  buf_size &= 0xcccccccc; }
    if (buf_size & 0xaaaaaaaa) bin_id ++;
   which gives the number of the highest set bit in buf_size.

*/
static int Get_Bin_ID(int buf_size)
{
  int bin_id = 0; /* By default, assume a zero size message */

  /* We subdivide this a little to catch the short messages
     early */
  if (buf_size > 4*ONE_KB) {
      if ( buf_size > MAX_MSG ) bin_id = NBINS - 1; /* The last bin is for
						       messages > 1024MB */
      else if ( buf_size > 512*ONE_MB ) bin_id = sz_513to1024M;
      else if ( buf_size > 256*ONE_MB ) bin_id = sz_257to512M;
      else if ( buf_size > 128*ONE_MB ) bin_id = sz_129to256M;
      else if ( buf_size >  64*ONE_MB ) bin_id = sz_65to128M;
      else if ( buf_size >  32*ONE_MB ) bin_id = sz_33to64M;
      else if ( buf_size >  16*ONE_MB ) bin_id = sz_17to32M;
      else if ( buf_size >   8*ONE_MB ) bin_id = sz_9to16M;
      else if ( buf_size >   4*ONE_MB ) bin_id = sz_5to8M;
      else if ( buf_size >     ONE_MB ) bin_id = sz_1to4M;
      else if ( buf_size > 512*ONE_KB ) bin_id = sz_513to1024K;
      else if ( buf_size > 256*ONE_KB ) bin_id = sz_257to512K;
      else if ( buf_size > 128*ONE_KB ) bin_id = sz_129to256K;
      else if ( buf_size >  64*ONE_KB ) bin_id = sz_65to128K;
      else if ( buf_size >  32*ONE_KB ) bin_id = sz_33to64K;
      else if ( buf_size >  16*ONE_KB ) bin_id = sz_17to32K;
      else if ( buf_size >   8*ONE_KB ) bin_id = sz_9to16K;
      else /*if ( buf_size >   4*ONE_KB )*/ bin_id = sz_5to8K;
  }
  else {
     if ( buf_size >  32 )  {
	 if      ( buf_size > ONE_KB ) bin_id = sz_1to4K;
	 else if ( buf_size > 512 )    bin_id = sz_513to1024;
	 else if ( buf_size > 256 )    bin_id = sz_257to512;
	 else if ( buf_size > 128 )    bin_id = sz_129to256;
	 else if ( buf_size >  64 )    bin_id = sz_65to128;
	 else 	                       bin_id = sz_33to64;
     }
     else {
	 if      ( buf_size >  16 )    bin_id = sz_17to32;
	 else if ( buf_size >   8 )    bin_id = sz_9to16;
	 else if ( buf_size >   4 )    bin_id = sz_5to8;
	 else if ( buf_size >   0  )   bin_id = sz_1to4;
	 else                          bin_id = sz_0;
     }
  }
  return bin_id;
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
MONITOR_EXT_WRAP_NAME(MPI_Allgather)(  void *sendbuf, int sendcount, MPI_Datatype sendtype,
        void *recvbuf, int recvcount, MPI_Datatype recvtype,
        MPI_Comm comm  )
{

  int bytes = Get_Msg_size(sendcount, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Allgather( sendbuf, sendcount, sendtype, recvbuf,
          recvcount, recvtype, comm );

}


int
MONITOR_EXT_WRAP_NAME(MPI_Allgatherv)(  void *sendbuf, int sendcount, MPI_Datatype sendtype,
        void *recvbuf, int *recvcount, int *displs, MPI_Datatype recvtype,
        MPI_Comm comm  )
{

  int bytes = Get_Msg_size(sendcount, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Allgatherv( sendbuf, sendcount, sendtype, recvbuf,
          recvcount, displs, recvtype, comm );

}


int
MONITOR_EXT_WRAP_NAME(MPI_Allreduce)( void *sendbuf, void *recvbuf, int count,
        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm )
{

  int bytes = Get_Msg_size(count, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Allreduce( sendbuf, recvbuf, count, datatype, op, comm );

}


int
MONITOR_EXT_WRAP_NAME(MPI_Alltoall)( void *sendbuf, int sendcount, MPI_Datatype sendtype,
		  void *recvbuf, int recvcnt, MPI_Datatype recvtype,
		  MPI_Comm comm )
{

  int bytes = Get_Msg_size(sendcount, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Alltoall( sendbuf, sendcount, sendtype, recvbuf,
		     recvcnt, recvtype, comm );

}


int
MONITOR_EXT_WRAP_NAME(MPI_Alltoallv)( void *sendbuf, int *sendcnts, int *sdispls,
		   MPI_Datatype sendtype, void *recvbuf, int *recvcnts,
		   int *rdispls, MPI_Datatype recvtype, MPI_Comm comm )
{

  int bytes = Get_Msg_size( *sendcnts, sendtype );
  hpmpi_store_metric(bytes);

  return PMPI_Alltoallv( sendbuf, sendcnts, sdispls, sendtype, recvbuf,
	      recvcnts, rdispls, recvtype, comm );

}



int
MONITOR_EXT_WRAP_NAME(MPI_Send)(  void *buf, int count, MPI_Datatype datatype, int dest,
        int tag, MPI_Comm comm )
{

  int bytes = Get_Msg_size(count, datatype );
  hpmpi_store_metric(bytes);

  return PMPI_Send( buf, count, datatype, dest, tag, comm);

}


int
MONITOR_EXT_WRAP_NAME(MPI_Recv)(  void *buf, int count, MPI_Datatype datatype, int dest,
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

