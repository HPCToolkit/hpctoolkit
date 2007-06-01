#include <stdlib.h>
#include <stdio.h>

#define ITERATIONS (1<<16)

int main(int argc, char **argv)
{
    int i;
    void *accum = NULL;

    for(i = 0; i < ITERATIONS; ++i) {
        /* allocate varying amount for funsies */
        void *dummy = malloc(ITERATIONS - i);

        accum += (accum - dummy);

        free(dummy);
    }

    printf("accum = %p\n", accum);

//    *((void **)(NULL)) = accum;

    return 0;
}
