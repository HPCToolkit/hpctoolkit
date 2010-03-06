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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#define LIMIT_OUTER 1000
#define LIMIT 100

#include <pthread.h>
#include <sys/time.h>

double spin(long depth)
{
  int i;
  double sum = 0;
  if (depth > 0) for(i=0;i<10;i++) sum += spin(depth-1);
  return sum;
}

void *pspin(void *t)
{
	int depth = (long) t;
	spin(depth);
	pthread_exit(0);
}

#define NUM_THREADS	1


int main(int argc, char **argv)
{
  pthread_t *threads;
  int t, rc;

  if (argc != 3) {
	printf("usage: threads depth\n");
	exit(-1);
  }
  int nthreads = atoi(argv[1]);
  int depth = atoi(argv[2]);

  threads = malloc(nthreads * sizeof(pthread_t *));

  for(t=0;t<nthreads;t++){
      printf("Creating thread %d\n", t);
      rc = pthread_create(&threads[t], NULL, pspin, (void *)(long) depth);
      if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
      }
   }
   printf("Main thread calls spin\n");
   spin(depth);
   printf("Main thread back from spin\n");
   for(t=0;t<nthreads;t++){
      printf("Joining thread %d\n", t);
      rc = pthread_join(threads[t],0);
      if (rc){
         printf("ERROR; return code from pthread_join() is %d\n", rc);
         exit(-1);
      }
   }
   return 0;
   //   exit(0);
}
