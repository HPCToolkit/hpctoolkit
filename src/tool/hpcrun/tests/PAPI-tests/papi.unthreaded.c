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
// Copyright ((c)) 2002-2009, Rice University 
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

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// ************************* PAPI stuff **********************

typedef char *caddr_t; // add this myself to see if it works

#include "papi.h"

#define EVENT              PAPI_TOT_CYC
#define DEF_THRESHOLD      1000000

#define errx(code,msg,...) do { \
  fprintf(stderr,msg "\n", ##__VA_ARGS__); \
  abort();\
} while(0)

int count    =  0;
int EventSet = -1;

int skip_thread_init = 0;

void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
  count += 1;
}

void
papi_setup(void)
{
  int threshold = DEF_THRESHOLD;

#if 0
  PAPI_set_debug(0x3ff);
#endif

  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT)
    errx(1,"Failed: PAPI_library_init");

  if (! skip_thread_init) {
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
      errx(1,"Failed: PAPI_thread_init");
  }

  if (PAPI_create_eventset(&EventSet) != PAPI_OK)
    errx(1, "PAPI_create_eventset failed");
  
  if (PAPI_add_event(EventSet, EVENT) != PAPI_OK)
    errx(1, "PAPI_add_event failed");

  if (PAPI_overflow(EventSet, EVENT, threshold, 0, my_handler) != PAPI_OK)
    errx(1, "PAPI_overflow failed");

  if (PAPI_start(EventSet) != PAPI_OK)
    errx(1, "PAPI_start failed");
}
  
void
papi_over(void)
{
  long_long values = -1;
  if (PAPI_stop(EventSet, &values) != PAPI_OK)
    errx(1,"PAPI_stop failed");
  PAPI_shutdown();
}

// ************************* Cycle burner ********************


#define LIMIT_OUTER 100
#define LIMIT 100000

void foob(double *x){
  *x = (*x) * 3.14 + atan(*x);
}

double cycles(void)
{
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
  return x+y;
}

// *************************** Main program ***************

int main(int argc, char *argv[])
{
  skip_thread_init = (argc > 1);

  papi_setup();

  double result = cycles();

  papi_over();

  printf("result = %g\n",result);
  printf("count = %d\n",count);
  return 0;
}
