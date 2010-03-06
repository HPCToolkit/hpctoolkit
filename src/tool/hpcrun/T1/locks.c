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
 * Simple test of spin locks, mutex locks and cond wait.  Increment a
 * global variable with above types of locks (or unlocked) and print
 * the final count.
 *
 * Usage: ./locks [num-iterations] [increment]
 *
 * $Id$
 */

#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEFAULT_NUM  10
#define DEFAULT_INC   1

pthread_spinlock_t spin;
pthread_mutex_t mutex;
pthread_cond_t cond;

long NUM, INC;

volatile long count1 = 0, count2 = 0, count3 = 0, count4 = 0;
volatile int  start1 = 0, start2 = 0, start3 = 0, start4 = 0;
volatile int  done0 = 0, done1 = 0, done2 = 0, done3 = 0, done4 = 0;

void
inc_nolock(void)
{
    long n;

    for (n = 1; n <= NUM; n++) {
	count1 += INC;
    }
}

void
inc_spin(void)
{
    long n;

    for (n = 1; n <= NUM; n++) {
	pthread_spin_lock(&spin);
	count2 += INC;
	pthread_spin_unlock(&spin);
    }
}

void
inc_mutex(void)
{
    long n;

    for (n = 1; n <= NUM; n++) {
	pthread_mutex_lock(&mutex);
	count3 += INC;
	pthread_mutex_unlock(&mutex);
    }
}

void
inc_cond_wait(void)
{
    pthread_mutex_lock(&mutex);
    do {
	pthread_cond_wait(&cond, &mutex);
	count4 += INC;
    } while (! done4);
    pthread_mutex_unlock(&mutex);
}

void
inc_cond_signal(void)
{
    struct timeval last, now;
    long n, count;

    for (n = 1; n <= NUM; n++) {
	pthread_mutex_lock(&mutex);
	count = count4;
	if (n == NUM)
	    done4 = 1;
	pthread_mutex_unlock(&mutex);
	pthread_cond_signal(&cond);
	gettimeofday(&last, NULL);
	while (count == count4) {
	    gettimeofday(&now, NULL);
	    if (now.tv_usec != last.tv_usec) {
		pthread_cond_signal(&cond);
		last = now;
	    }
	}
    }
}

void *
side_thread(void *p)
{
    done0 = 1;

    while (! start1) ;
    inc_nolock();
    done1 = 1;

    while (! start2) ;
    inc_spin();
    done2 = 1;

    while (! start3) ;
    inc_mutex();
    done3 = 1;

    while (! start4) ;
    inc_cond_signal();
    done4 = 1;

    return (NULL);
}

int
main(int argc, char **argv)
{
    pthread_t td;

    if (argc < 2 || sscanf(argv[1], "%ld", &NUM) < 1)
	NUM = DEFAULT_NUM;
    if (argc < 3 || sscanf(argv[2], "%ld", &INC) < 1)
	INC = DEFAULT_INC;

    printf("NUM = %ld, INC = %ld\n", NUM, INC);
    printf("ans = %ld\n", 2 * NUM * INC);

    if (pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE) != 0)
	errx(1, "pthread_spin_init failed");

    if (pthread_mutex_init(&mutex, NULL) != 0)
	errx(1, "pthread_mutex_init failed");

    if (pthread_cond_init(&cond, NULL) != 0)
	errx(1, "pthread_cond_init failed");

    if (pthread_create(&td, NULL, side_thread, NULL) != 0)
	errx(1, "pthread_create failed");
    while (! done0) ;

    printf("\nphase 1: increment unlocked\n");
    printf("count1 = %ld\n", count1);
    start1 = 1;
    inc_nolock();
    while (! done1) ;
    printf("count1 = %ld\n", count1);

    printf("\nphase 2: increment spin locked\n");
    printf("count2 = %ld\n", count2);
    start2 = 1;
    inc_spin();
    while (! done2) ;
    printf("count2 = %ld\n", count2);

    printf("\nphase 3: increment mutex locked\n");
    printf("count3 = %ld\n", count3);
    start3 = 1;
    inc_mutex();
    while (! done3) ;
    printf("count3 = %ld\n", count3);

    printf("\nphase 4: increment cond wait\n");
    printf("count4 = %ld\n", count4);
    start4 = 1;
    inc_cond_wait();
    while (! done4) ;
    printf("count4 = %ld\n", count4);

    return (0);
}
