objs = test-spinlock test-atomic test-executable-path
CFLAGS =-g
all: $(objs)

test-spinlock: test-spinlock.c 
	gcc $(CFLAGS) -I../utilities -o test-spinlock test-spinlock.c ../utilities/spinlock.c

test-atomic: test-atomic.c
	gcc $(CFLAGS) -I../utilities -o test-atomic test-atomic.c 

test-executable-path: test-executable-path.c
	gcc $(CFLAGS) -I../utilities -o test-executable-path test-executable-path.c ../utilities/executable-path.c

clean: 
	/bin/rm -f $(objs)
