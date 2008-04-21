#include <stdio.h>

#include "spinlock.h"

spinlock_t l = SPINLOCK_UNLOCKED;

const 
char *lock_status()
{
	if (spinlock_is_locked(&l)) return "locked";
	return "unlocked";
}

void 
print_lock_status()
{
	printf("spinlock status = %s\n", lock_status(&l));
}

int 
main(int argc, char **argv)
{
	printf("initial lock state: ");
	print_lock_status();

	printf("locking spinlock\n");
	spinlock_lock(&l);
	print_lock_status();

	printf("unlocking spinlock\n");
	spinlock_unlock(&l);
	print_lock_status();

	printf("locking spinlock\n");
	spinlock_lock(&l);
	print_lock_status();

	printf("locking spinlock again (should hang)\n");
	spinlock_lock(&l);
	printf("program failed!\n");
}
