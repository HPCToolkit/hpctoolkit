all: test-spinlock test-atomic

test-spinlock: test-spinlock.c 
	gcc -I../utilities -o test-spinlock test-spinlock.c ../utilities/spinlock.c

test-atomic: test-atomic.c
	gcc -I../utilities -o test-atomic test-atomic.c 

clean: 
	/bin/rm -f test-spinlock
