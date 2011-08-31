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

#include <mpi.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#define LIMIT_OUTER 1000
#define LIMIT 100

double msin(double x){
	int i;
        double xx;

        xx = x;
	for(i=0;i<1000;i++) x++;
	return xx/(xx * 1000.);
}

double mcos(double x){
	msin(x);
	return x/(x * 100.);
}


double mlog(double x){
	mcos(x);
	return x/(x * 10.);
}

void foob(double *x){
#ifndef NO
  *x = (*x) * 3.14 + mlog(*x);
#else
  *x = (*x) * 3.14 + log(*x);
#endif
}
void nobar(void){
  double x,y;
  int i,j;

  x = 2.78;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + sin(y);
      x = log(y) + cos(x);
    }
  }
}

int bar(int myRank){
  double x,y;
  int i,j;

  x = 2.78;
  foob(&x);
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      y = x * x + msin(y);
      x = mlog(y) + mcos(x);
    }
  }
  // printf("[%d] Before barrier\n",myRank);
  MPI_Barrier(MPI_COMM_WORLD);
  if (myRank == 3){
    printf("x = %g, y = %g\n",x,y);
  }
  return 0;
}

void csprof_set_rank(int rank);

int main(int argc, char *argv[]){
  int myRank, commSize;

  MPI_Init( &argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &commSize);
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank );

  // csprof_set_rank(myRank); // temporary until gen soln can be found

  bar(myRank);
  MPI_Barrier(MPI_COMM_WORLD);
  if (myRank == 3){
    printf("Ending TMPI\n");
  }
  fflush(stdout);
  MPI_Finalize();
  return 0;
}
