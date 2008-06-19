/*
 * Wrapper for _exit.
 */

#include <stdio.h>

extern unsigned long csprof_nm_addrs[];

void
__wrap__exit(int status)
{
    printf("==> in my wrap _exit: status = %d, csprof addr = 0x%lx\n",
	   status, csprof_nm_addrs[0]);

    __real__exit(status);
}
