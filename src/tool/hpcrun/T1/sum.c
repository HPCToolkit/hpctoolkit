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
 * A library that defines several functions that all churn cycles.
 * This is a way to create several intervals, so that sampling calls
 * dl_iterate_phdr() many times.
 *
 * Compile twice, with/without LIBSUM_TWO defined.
 *
 * $Id$
 */

#ifdef LIBSUM_TWO
#define MAKE_SUM(num)  SUM_HELP(2, num)
#else
#define MAKE_SUM(num)  SUM_HELP(1, num)
#endif

#define SUM_HELP(label, suffix)		\
double sum_ ## label ## _ ## suffix (long num) {  \
    double sum = 0.0;			\
    long k;				\
    for (k = 1; k <= num; k++) {	\
	sum += (double)k;		\
    }					\
    return (sum);			\
}

MAKE_SUM(0)
MAKE_SUM(1)
MAKE_SUM(2)
MAKE_SUM(3)
MAKE_SUM(4)
MAKE_SUM(5)
MAKE_SUM(6)
MAKE_SUM(7)
MAKE_SUM(8)
MAKE_SUM(9)
MAKE_SUM(10)
MAKE_SUM(11)
MAKE_SUM(12)
MAKE_SUM(13)
MAKE_SUM(14)
MAKE_SUM(15)
MAKE_SUM(16)
MAKE_SUM(17)
MAKE_SUM(18)
MAKE_SUM(19)
MAKE_SUM(20)
MAKE_SUM(21)
MAKE_SUM(22)
MAKE_SUM(23)
MAKE_SUM(24)
MAKE_SUM(25)
MAKE_SUM(26)
MAKE_SUM(27)
MAKE_SUM(28)
MAKE_SUM(29)
MAKE_SUM(30)
MAKE_SUM(31)
MAKE_SUM(32)
MAKE_SUM(33)
MAKE_SUM(34)
MAKE_SUM(35)
MAKE_SUM(36)
MAKE_SUM(37)
MAKE_SUM(38)
MAKE_SUM(39)
MAKE_SUM(40)
MAKE_SUM(41)
MAKE_SUM(42)
MAKE_SUM(43)
MAKE_SUM(44)
MAKE_SUM(45)
MAKE_SUM(46)
MAKE_SUM(47)
MAKE_SUM(48)
MAKE_SUM(49)
MAKE_SUM(50)
MAKE_SUM(51)
MAKE_SUM(52)
MAKE_SUM(53)
MAKE_SUM(54)
MAKE_SUM(55)
