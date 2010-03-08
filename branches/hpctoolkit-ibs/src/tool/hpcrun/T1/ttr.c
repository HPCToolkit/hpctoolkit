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
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

static double accum = 0.;
static const double seed1 = 3.14;
static const double seed2 = 2.78;

static double
x_upd(double x)
{
  return cos(x*x);
}

static double
y_upd(double y)
{
  double t1 = tan(y);
  if (t1 == 0.0) t1 =1.0e-06;
  double t2 = tan(t1);
  //  printf("tan factor = %g\n", t2);
  t2 = pow(abs(t2), .335);
  //  printf("y upd = %g\n", t2);
  return  t2;
}


static double
work3_d(double z)
{
}

static double
work2_d(double x, double y)
{
  double t1, t2, t3;

  t1 = sin(x) + log(y);
  t2 = x + y;
  t3 = sqrt(abs(x*y));

  return t1 * t2 + t3;
}

static void
work1(int in)
{
  static double x = 3.14;
  static double y = 2.78;

  // printf("in = %d, x = %g, y = %g\n", in, x, y);

  for(int i=0; i<in; i++){
    accum += work2_d(x,y);
    x += x_upd(x);
    y -= y_upd(y);
  }
}

static const int NTIMES = 10;

int
main(int argc, char *argv[])
{
  for(int i=0; i < NTIMES; i++) {
    work1(i);
  }
  printf("Ans = %g\n", accum);
}
