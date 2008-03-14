#include <stdio.h>

#include "atomic.h"

long l = 0;
long m = 1;
long *p = &l;

void 
print_long_status()
{
	printf("long value = %d\n", l);
}

int 
main(int argc, char **argv)
{
	printf("initial long state: ");
	print_long_status();

	printf("p = %p, *p = %d\n", p, *p);
	printf("swapping p; old value = %p\n", csprof_atomic_swap_p(&p, &m));
	printf("p = %p, *p = %d\n", p, *p);

	printf("swapping long with 2; old value = %d\n", csprof_atomic_swap_l(&l, 2));
	print_long_status();

	printf("swapping long with 3; old value = %d\n", csprof_atomic_swap_l(&l, 3));
	print_long_status();

	printf("incrementing long; old value = %d\n", csprof_atomic_increment(&l));
	print_long_status();
}
