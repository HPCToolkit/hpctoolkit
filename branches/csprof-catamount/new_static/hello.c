/*
 *  A small program to see how gcc handles the link line.
 *
 *  $Id: hello.c,v 1.1 2007/06/18 02:54:43 krentel Exp krentel $
 */

#include <stdio.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    printf("hello, world\n");
    _exit(42);

    return (0);
}
