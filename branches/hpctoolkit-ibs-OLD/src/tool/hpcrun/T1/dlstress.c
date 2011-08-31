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
 * Stress test of dlopen, dlclose and dl_iterate_phdr.
 *
 * Run two threads of dlopen/dlclose (writers), dl_iterate_phdr
 * (readers via sampling), both readers and writers, or just plain
 * cycles.  The libsum library has many separate functions, so
 * sampling causes many calls to dl_iterate_phdr.  This is a stress
 * test of the csprof and system locks for dlopen and friends.
 *
 * Usage: ./dlstress [program-time] [crwx] [crwx]
 *
 * where "program-time" is in seconds, and
 *
 *   c = churn cycles only (just one dlopen),
 *   r = run loop of dl_iterate_phdr (reader),
 *   w = run loop of dlopen and dlclose (writer),
 *   x = run loop of readers and writers.
 *
 * $Id$
 */

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define PROGRAM_TIME  6
#define LIBDIR  "/usr/lib64"
#define SIZE    40

void *handle[SIZE];
char *name[SIZE] = {
    "libcrypt.so",
    "libcurl.so",
    "libelf.so",
    "libform.so",
    "libfreetype.so",
    "libpython2.4.so",
    "libresolv.so",
    "libruby.so",
    "libtiff.so",
    "libtk8.4.so",
    "libwrap.so",
    "libz.so",
    "libGL.so",
    "libICE.so",
    "libX11.so",
    "libXaw.so",
    "libXpm.so",
    "libXv.so",
    NULL, NULL,
    NULL, NULL,
};

struct loop_args {
    char *label;
    char *type;
    int  lib;
    int  cycle;
    int  read;
    int  write;
    int  start;
    int  inc;
    long limit;
};

struct timeval start_time;
int program_time;

void
do_cycles(struct loop_args *la, void *handle, int num)
{
    double (*fcn)(long), sum;
    char buf[200];

    sprintf(buf, "sum_%d_%d", la->lib, num);
    fcn = dlsym(handle, buf);
    sum = (*fcn)(la->limit);
    printf("%s: %s = %g\n", la->label, buf, sum);
}

void
do_loop(struct loop_args *la)
{
    struct timeval now;
    void *addr, *sum_handle;
    char buf[500];
    int j, k, num;

    addr = NULL;  sum_handle = NULL;
    for (num = 1;; num++) {
	printf("\nbegin %s: %s iteration %d ...\n", la->label, la->type, num);
	/*
	 * Get libsum handle for read or cycle test.
	 * Cycle opens the library only once.
	 */
	if (la->read || (la->cycle && num == 1)) {
	    sprintf(buf, "./libsum%d.so", la->lib);
	    sum_handle = dlopen(buf, RTLD_LAZY);
	    if (sum_handle == NULL)
		errx(1, "dlopen (%s) failed", buf);
	    sprintf(buf, "sum_%d_0", la->lib);
	    addr = dlsym(sum_handle, buf);
	    if (addr == NULL)
		errx(1, "dlsym (%s) failed", buf);
	    printf("%s: libsum%d addr = %p\n", la->label, la->lib, addr);
	}

	/* Cycle or read test. */
	if (! la->write) {
	    for (j = 1; j <= 50; j++) {
		do_cycles(la, sum_handle, j);
	    }
	    goto finish;
	}

	/* Open */
	j = 1;
	for (k = la->start; name[k] != NULL; k += la->inc) {
	    if (la->read) {
		do_cycles(la, sum_handle, j);
		j++;
	    }
	    if (handle[k] == NULL) {
		sprintf(buf, "%s/%s", LIBDIR, name[k]);
		handle[k] = dlopen(buf, RTLD_NOW);
		printf("%s: open %s: %s\n", la->label, name[k],
		       (handle[k] != NULL) ? "success" : "FAILURE");
	    } else {
		printf("%s: open %s: %s\n", la->label, buf, "STILL LOADED");
	    }
	}

	/* Close */
	for (k = la->start; name[k] != NULL; k += la->inc) {
	    if (la->read) {
		do_cycles(la, sum_handle, j);
		j++;
	    }
	    if (handle[k] != NULL) {
		if (dlclose(handle[k]) == 0) {
		    printf("%s: close %s: success\n", la->label, name[k]);
		    handle[k] = NULL;
		} else {
		    printf("%s: close %s: FAILURE\n", la->label, name[k]);
		}
	    }
	}
	/*
	 * Close in reverse order, in case some library will only
	 * unload after its successors are also unloaded.
	 */
	for (; k >= 0; k -= la->inc) {
	    if (handle[k] != NULL) {
		if (dlclose(handle[k]) == 0) {
		    printf("%s: close %s: success\n", la->label, name[k]);
		    handle[k] = NULL;
		} else {
		    printf("%s: close %s: FAILURE\n", la->label, name[k]);
		}
	    }
	}

    finish:
	if (la->read) {
	    dlclose(sum_handle);
	    addr = (void *) (((long)addr) & ~4095L);
	    mmap(addr, 4096, PROT_READ, MAP_ANON|MAP_PRIVATE|MAP_FIXED, -1, 0);
	}

	gettimeofday(&now, NULL);
	if (now.tv_sec >= start_time.tv_sec + program_time)
	    break;
    }
}

void
init_args(struct loop_args *la, char *label, int lib, long limit)
{
    memset(la, 0, sizeof(struct loop_args));
    la->label = label;
    la->lib = lib;
    la->limit = limit;
    la->start = 0;
    la->inc = 1;
}

void
parse_args(struct loop_args *la, char *label, char *arg)
{
    switch (arg[0]) {
    case 'C': case 'c':
	printf("%s thread: libsum cycles only\n", label);
	la->type = "cycle only";
	la->cycle = 1;
	break;
    case 'R': case 'r':
	printf("%s thread: libsum (reader)\n", label);
	la->type = "read";
	la->read = 1;
	break;
    case 'W': case 'w':
	printf("%s thread: dlopen/dlclose (writer)\n", label);
	la->type = "write";
	la->write = 1;
	break;
    case 'X': case 'x':
	printf("%s thread: dlopen/dlclose/libsum (writer/reader)\n", label);
	la->type = "read/write";
	la->read = 1;
	la->write = 1;
	break;
    default:
	errx(1, "unknown %s thread type: %s\n", label, arg);
	break;
    }
}

void *
side_thread(void *data)
{
    do_loop(data);
    printf("\nside thread exit\n");
    return (NULL);
}

int
main(int argc, char **argv)
{
    struct loop_args main_args, side_args;
    pthread_t td;
    int k;

    if (argc < 2 || sscanf(argv[1], "%d", &program_time) < 1)
	program_time = PROGRAM_TIME;
    printf("program time = %d\n", program_time);

    for (k = 0; k < SIZE; k++)
	handle[k] = NULL;

    gettimeofday(&start_time, NULL);

    init_args(&main_args, "main", 1, 200000);
    init_args(&side_args, "side", 2, 300000);

    if (argc <= 2) {
	printf("single thread: dlopen/dlclose/libsum (writer/reader)\n");
	main_args.read = 1;
	main_args.write = 1;
    }
    if (argc >= 3) {
	parse_args(&main_args, "main", argv[2]);
    }
    if (argc >= 4) {
	parse_args(&side_args, "side", argv[3]);
    }

    if (main_args.write && side_args.write) {
	main_args.start = 0;
	main_args.inc = 2;
	side_args.start = 1;
	side_args.inc = 2;
    }

    if (argc >= 4) {
	if (pthread_create(&td, NULL, side_thread, &side_args) != 0)
	    errx(1, "pthread_create failed");
    }
    do_loop(&main_args);

    if (argc >= 4) {
	printf("\nmain: waiting on pthread join ...\n");
	pthread_join(td, NULL);
    }
    printf("done\n");

    return (0);
}
