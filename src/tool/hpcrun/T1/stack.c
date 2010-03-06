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

/*
 * Use macros to make several stack frames.
 */

#include <sys/time.h>
#include <stdio.h>

#define MAKE_FCN(fcn_name, callee, suffix)				\
long fcn_name(long low_##suffix, long high_##suffix) {			\
    long i_##suffix, mid_##suffix, mid1_##suffix, tot_##suffix;	       	\
    if (low_##suffix > high_##suffix)					\
	return (0);							\
    tot_##suffix = 0;							\
    mid1_##suffix = (4 * low_##suffix + high_##suffix)/5;		\
    for (i_##suffix = low_##suffix;					\
	 i_##suffix <= mid1_##suffix;					\
	 i_##suffix ++)							\
	tot_##suffix += i_##suffix;					\
    mid_##suffix = (mid1_##suffix + 1 + high_##suffix)/2;		\
    tot_##suffix += callee(mid1_##suffix + 1, mid_##suffix);		\
    tot_##suffix += callee(mid_##suffix + 1, high_##suffix);		\
    return (tot_##suffix);						\
}

long
base(long low, long high)
{
    long i, tot;

    tot = 0;
    for (i = low; i <= high; i++)
	tot += i;

    return (tot);
}

MAKE_FCN(sum_1, base, 1)
MAKE_FCN(sum_2, sum_1, 2)
MAKE_FCN(sum_3, sum_2, 3)
MAKE_FCN(sum_4, sum_3, 4)
MAKE_FCN(sum_5, sum_4, 5)
MAKE_FCN(sum_6, sum_5, 6)
MAKE_FCN(sum_7, sum_6, 7)
MAKE_FCN(sum_8, sum_7, 8)
MAKE_FCN(sum_9, sum_8, 9)

int
main(int argc, char **argv)
{
    struct timeval start, now;
    long ans, count, len, n;

    if (argc < 2 || sscanf(argv[1], "%ld", &len) < 1)
	len = 6;
    if (argc < 3 || sscanf(argv[2], "%ld", &n) < 1)
	n = 1000000;

    gettimeofday(&start, NULL);
    printf("start time: sec = %d, usec = %d\n",
	   start.tv_sec, start.tv_usec);
    count = 0;
    do {
	ans = sum_9(1, n);
	count++;
	gettimeofday(&now, NULL);
    } while (now.tv_sec < start.tv_sec + 5);

    printf("end time:   sec = %d, usec = %d\n",
	   now.tv_sec, now.tv_usec);
    printf("ans sum_9(%d, %ld) = %ld, count = %ld\n",
	   1, n, ans, count);

    return (0);
}
