/*
 * Simple test of dlopen and dlclose.
 *
 * This test can cause problems for hpcrun at high sampling rates
 * (10-50,000 PAPI_TOT_CYC) if we take a sample at just the wrong
 * place inside the dlopen() library.
 *
 * $Id$
 */

#include <dlfcn.h>
#include <stdio.h>

#define LIBDIR  "/usr/lib64"
#define SIZE    20

char *name[] = {
    "libelf.so",
    "libz.so",
    "libresolv.so",
    "libkdeinit_konqueror.so",
    "libpython2.4.so",
    "libruby.so",
    "libtk8.4.so",
    "libtiff.so",
    "libX11.so",
    NULL,
};

void *handle[SIZE];

int
main(int argc, char **argv)
{
    char buf[2000];
    int num, ret, n, k;

    if (argc < 2 || sscanf(argv[1], "%d", &num) < 1)
	num = 20;

    for (k = 0; k < SIZE; k++)
	handle[k] = NULL;

    for (n = 1; n <= num; n++) {
	printf("\nbegin iteration %d ...\n", n);

	/* Open */
	for (k = 0; name[k] != NULL; k++) {
	    if (handle[k] == NULL) {
		sprintf(buf, "%s/%s", LIBDIR, name[k]);
		handle[k] = dlopen(buf, RTLD_NOW);
		printf("open %s: %s\n", buf,
		       (handle[k] != NULL) ? "success" : "failure");
	    } else {
		printf("open %s: %s\n", buf, "still loaded");
	    }
	}

	/* Close */
	for (k = 0; name[k] != NULL; k++) {
	    if (handle[k] != NULL) {
		ret = dlclose(handle[k]);
		printf("close %s: %s\n", name[k],
		       (ret == 0) ? "success" : "failure");
		if (ret == 0)
		    handle[k] = NULL;
	    }
	}
	/*
	 * Close in reverse order, in case some library will only
	 * unload after its successors are also unloaded.
	 */
	for (k = SIZE - 1; k >= 0; k--) {
	    if (handle[k] != NULL) {
		ret = dlclose(handle[k]);
		printf("close %s: %s\n", name[k],
		       (ret == 0) ? "success" : "failure");
		if (ret == 0)
		    handle[k] = NULL;
	    }
	}
    }

    return (0);
}
