#include <stdio.h>
#include <assert.h>

#include <lib/prof-lean/atomic-op.h>

#define L_INIT 17
#define M_INIT 41

long l = L_INIT;
long m = M_INIT;

long *p = &l;

void 
print_long_status()
{
	printf("value *(%p) = %d, *(%p) = %d\n", &l, l, &m, m);
}

int 
main(int argc, char **argv)
{
	printf("initial long state: ");
	print_long_status();
	assert(p == &l && *p == L_INIT);

	printf("swapping *(%p) = %p with %p, ", &p, p, &m); 
	long *oldptr = fetch_and_store_ptr(&p, &m);
 	printf("old value = %p, new value = %p\n", oldptr, p); 
	assert(p == &m && *p == M_INIT);
	print_long_status();

	printf("swapping *(%p) = %d with 2\n", &l, l); 
	long old = fetch_and_store(&l, 2);
	assert(old == L_INIT && l == 2);
	print_long_status();

	printf("swapping *(%p) = %d with 3\n", &l, l); 
	old = fetch_and_store(&l, 3);
	assert(old == 2 && l == 3);
	print_long_status();

	printf("fetch-and-add(&l, 4),  old value = %d\n", old = fetch_and_add(&l, 4));
	assert(old == 3 && l == 7);
	print_long_status();

#if    defined(LL_BODY) && defined (SC_BODY)
	long _val = 20;
        printf("load_linked(%p) = %d, store_conditional(%p,20) = %d\n", &l, old = load_linked(&l), &l, store_conditional(&l,_val));
	assert(old == 7 && l == 20); /* prior sc should succeed */
	print_long_status();
	_val = 21;
        printf("store_conditional(%p,%d) = %d\n", &l, _val, old = store_conditional(&l,_val));
	assert(old == 0 && l == 20); /* prior sc should fail */
	print_long_status();
#endif
}

