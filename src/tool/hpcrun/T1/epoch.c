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
 * Test of generating multiple epochs.  Open a bunch of libraries,
 * close one to make a hole, open libsum1.so and churn cycles in it,
 * close libsum1.so and then open libsum2.so.  There's no guarantee,
 * but in practice both libsum1.so and libsum2.so are loaded in the
 * same place in the hole, and thus it produces multiple epochs.
 *
 * Usage: ./epoch [num-iterations]
 *
 * $Id$
 */

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>

#define LIBDIR  "/usr/lib64"
#define SIZE    40

char *name[] = {
    "libcrypt.so",
    "libcurl.so",
    "libelf.so",
    "libform.so",
    "libfreetype.so",
    "libpython2.4.so",
    "libruby.so",
    "libtiff.so",
    "libtk8.4.so",
    "libwrap.so",
    "libz.so",
    "libresolv.so",
    "libGL.so",
    "libICE.so",
    "libX11.so",
    "libXaw.so",
    "libXpm.so",
    "libXv.so",
    NULL, NULL,
};

void *handle[SIZE];

void
do_cycles(int index)
{
    double (*fcn)(long), sum;
    void *sum_handle;
    char lib[200], name[200], *err_str;
    int n, ret;

    sprintf(lib, "./libsum%d.so", index);
    sum_handle = dlopen(lib, RTLD_LAZY);
    printf("open %s: %s\n", lib, sum_handle ? "success" : "failure");
    for (n = 1; n <= 40; n++) {
	sprintf(name, "sum_%d_%d", index, n);
	dlerror();
	fcn = dlsym(sum_handle, name);
	if ((err_str = dlerror()) != NULL)
	    errx(1, "dlsym failed: %s", err_str);
	sum = (*fcn)(5000000);
	printf("%s: addr = %p, sum = %g\n", name, fcn, sum);
    }
    ret = dlclose(sum_handle);
    printf("close %s: %s\n", lib, ret ? "failure" : "success");
}

int
main(int argc, char **argv)
{
    char buf[500];
    int num, ret, n, k;

    if (argc < 2 || sscanf(argv[1], "%d", &num) < 1)
	num = 8;

    for (k = 0; name[k] != NULL; k++) {
	sprintf(buf, "%s/%s", LIBDIR, name[k]);
	handle[k] = dlopen(buf, RTLD_NOW);
	printf("open %s: %s\n", buf, handle[k] ? "success" : "failure");
    }

    k = 11;
    ret = dlclose(handle[k]);
    printf("close %s: %s\n", name[k], ret ? "failure" : "success");

    for (n = 1; n <= num; n++) {
	printf("\nbegin iteration %d ...\n", n);
	do_cycles(1);
	do_cycles(2);
    }

    return (0);
}
