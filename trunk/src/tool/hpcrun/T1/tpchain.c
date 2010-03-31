// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/T1/tp.c $
// $Id: tp.c 2770 2010-03-06 02:54:44Z tallent $
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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#define LIMIT_OUTER 1000
#define LIMIT 100

#include <pthread.h>
#include <sys/time.h>

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

int bar(){
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
  // printf("x = %g, y = %g\n",x,y);
  return 0;
}

#define DEFAULT_NUM_THREADS 1


void *oneThread(void *val) {
  long lval = (long) val;
  int rc;
  pthread_t kid;
  printf("starting thread %ld\n", lval);
  if (lval != 0) {
    rc = pthread_create(&kid, NULL, oneThread, (void *)(long)(lval-1));
    if (rc){
      printf("ERROR; return code from pthread_create() in thread %ld is %d\n", 
	     lval, rc);
       exit(-1);
    }
  }
  bar();
  printf("finishing thread %ld\n", lval);
}

int
main(int argc, char *argv[])
{
  int nthreads = DEFAULT_NUM_THREADS;

  if (argc >= 2){
    nthreads = atoi(argv[1]);
  }
  printf("Num threads = %d\n", nthreads);
  oneThread((void *) (long) nthreads-1);

  return 0;
}
