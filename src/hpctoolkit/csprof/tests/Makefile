objs = test-spinlock test-atomic-ops test-executable-path
processor:=`uname -p`
CFLAGS =-g -D__${processor}__

all: $(objs)

test-spinlock: test-spinlock.c 
	gcc $(CFLAGS) -I../utilities -o test-spinlock test-spinlock.c ../utilities/spinlock.c

test-atomic-ops: test-atomic-ops.c
	gcc $(CFLAGS) -I../utilities -o test-atomic-ops test-atomic-ops.c 

test-executable-path: test-executable-path.c
	gcc $(CFLAGS) -I../utilities -o test-executable-path test-executable-path.c ../utilities/executable-path.c

clean: 
	/bin/rm -f $(objs)
