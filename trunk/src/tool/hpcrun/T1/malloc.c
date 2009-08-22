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
