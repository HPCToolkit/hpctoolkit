#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

/* based off of some code by Paul Drongowski */

#define KILOBYTE 1024

int stride = 128;
long chases;
unsigned char *chase_array;

void
initialize_pointer_chase(long bytes)
{
    long i, half;
    unsigned char **p, **p2;
    long stride8;

    chase_array = (unsigned char *)malloc(bytes);
    
    chases = bytes/stride;
    half = chases/2;
    stride8 = stride/8;
    p = (unsigned char **)&chase_array[0];
    p2 = (unsigned char **)&chase_array[bytes/2];

    for(i=0; i<half; ++i) {
        *p = (unsigned char *) p2;
        *p2 = (unsigned char *) (p + stride8);
        p += stride8;
        p2 += stride8;
    }

    *(p2 - stride8) = (unsigned char *)0;
}

void
pointer_chase(long iterations, long kilobytes)
{
    long i;

    initialize_pointer_chase(kilobytes * KILOBYTE);
 
    for(i=0; i<iterations; ++i) {
        unsigned char **p = (unsigned char **)chase_array;

        while(p = (unsigned char **) *p);
    }

    free(chase_array);
}

/* allocate a bunch of memory and wander through it */
void
memory_stride(long iterations, long kb_to_alloc)
{
    long pagesize = sysconf(_SC_PAGE_SIZE);
    long kb_per_page = pagesize / KILOBYTE;
    long bytes = kb_to_alloc * KILOBYTE;
    char *memhunk = (char *)malloc(bytes);

    printf("Striding through %ld KB with %ld iterations\n",
           kb_to_alloc, iterations);

    /* stride through the memory */
    for(; --iterations; ) {
        long i;

        for(i=0; i<bytes; i+= pagesize) {
            memhunk[i] = 'f';
        }
    }

    free(memhunk);
}

int
main(int argc, char **argv)
{
    long kilobytes;
    long iterations;
    long junk;

    if(argc != 3) {
        printf("Usage: %s size(KB) iter\n", argv[0]);
        exit(1);
    }

    kilobytes = atol(argv[1]);
    iterations = atol(argv[2]);

    pointer_chase(iterations, kilobytes);

    return 0;
}
