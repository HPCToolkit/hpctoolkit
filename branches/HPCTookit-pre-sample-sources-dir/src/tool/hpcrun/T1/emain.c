/*
 * Main program to link with libearly.so.
 *
 * $Id$
 */

#include <stdio.h>
#include <unistd.h>

void early_fcn(void);

int
main(int argc, char **argv)
{
    printf("==> main begin\n");
    sleep(2);
    early_fcn();
    sleep(2);
    printf("==> main end\n");

    return (0);
}
