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
 *  Run a loop malloc(), sprintf() and free() to test deadlock in
 *  hpctoolkit.
 */

#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE  100

void *addr[SIZE];

void
do_cycles(int len)
{
    struct timeval start, last, now;
    long count, total;
    int k, l;

    gettimeofday(&start, NULL);
    last = start;

    count = 0;
    total = 0;
    do {
	for (k = 0; k < SIZE; k++) {
	    addr[k] = malloc(1024);
	    if (addr[k] == NULL)
		errx(1, "malloc failed");
	}
#if 0
	for (k = 0; k < SIZE; k++) {
	    l = sprintf(addr[k], "addr[%d] = %p, %x, %g",
			k, addr[k], 0x64ab0023, 3.14159);
	    if (l > 800)
		errx(1, "sprintf too long");
	}
#endif
	for (k = 0; k < SIZE; k++) {
	    free(addr[k]);
	}
	count++;
	total++;
	gettimeofday(&now, NULL);
        if (now.tv_sec > last.tv_sec) {
            printf("t = %d, count = %ld, total = %ld\n",
                   now.tv_sec - start.tv_sec, count, total);
            last = now;
            count = 0;
        }
    } while (now.tv_sec < start.tv_sec + len);
}

int
main(int argc, char **argv)
{
    int len;

    if (argc < 2 || sscanf(argv[1], "%d", &len) < 1)
	len = 10;
    printf("len = %d\n", len);

    do_cycles(len);

    printf("done\n");
    return (0);
}
