CFLAGS = -O3 -g -Wall -I.. -I../../../../BUILD/src/include -std=c99
LOCK_OBJS = lock-test.o mcs-lock.o
RWLOCK_OBJS = rwlock-test.o pfq-rwlock.o mcs-lock.o

all: lock-test rwlock-test

lock-test: $(LOCK_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LOCK_OBJS) -pthread -lm

lock-test.o: lock-test.c
	$(CC) $(CFLAGS) -c -o $@ $<

rwlock-test: $(RWLOCK_OBJS)
	$(CC) $(CFLAGS) -o $@ $(RWLOCK_OBJS) -pthread -lm

rwlock-test.o: rwlock-test.c
	$(CC) $(CFLAGS) -c -o $@ $<

mcs-lock.o: ../mcs-lock.c
	$(CC) $(CFLAGS) -c -o $@ $<

pfq-rwlock.o: ../pfq-rwlock.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o lock-test rwlock-test
