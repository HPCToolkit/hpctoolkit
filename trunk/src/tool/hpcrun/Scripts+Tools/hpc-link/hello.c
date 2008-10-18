/*
 * Hello, world program to be wrapped.
 */

#include <stdio.h>

int
main(int argc, char **argv)
{
    printf("hello, world: argc = %d\n", argc);

    _exit(argc);
}
