// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <stdio.h>
#define YESMPI 0
#ifdef YESMPI
#include <mpi.h>
#include <catamount/data.h>
#include <rca_lib.h>
#endif

void papi_pulse_fini(void);

long fib(long n) 
{
	if (n > 2) {
		return fib(n-1) + fib(n-2);
	} else return 1;
}
int main(int argc, char **argv)
{
	int myid;
#ifdef YESMPI
	rca_mesh_coord_t coords;
#endif
	long n, result; 

#ifdef YESMPI
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
#else
	myid = 0;
#endif

	if (argc < 2) {
	    if (myid == 0) printf("not enough arguments\n");
#ifdef YESMPI
	    MPI_Finalize();
#endif
	    exit(-1);
	}
	n = atoi(argv[1]);

#ifdef YESMPI
	rca_get_meshcoord(_my_pnid, &coords);
	printf("MPI rank %d maps to physical node %d (0x%x), mesh coordinate = [%d,%d,%d]\n", 
		myid, _my_pnid, _my_pnid, coords.mesh_x, coords.mesh_y, coords.mesh_z);
#else
	printf("MPI rank %d \n", myid );
#endif

#if 0
	result = 0;
#else
	result = fib(n);
#endif

#ifdef YESMPI
	MPI_Finalize();
#endif
	if (myid == 0) printf("fib(%ld) = %ld\n",n, result);
#if 0
	papi_pulse_fini();
#endif
}

