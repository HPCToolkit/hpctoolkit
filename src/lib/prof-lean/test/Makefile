CFLAGS = -O3 -g -Wall -I.. -I../../../../BUILD/src/include -std=c99
LOCK_OBJS = lock-test.o mcs-lock.o
PFQ_OBJS = pfq-test.o pfq-rwlock.o mcs-lock.o

all: lock-test pfq-test

lock-test: $(LOCK_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LOCK_OBJS) -pthread -lm

lock-test.o: lock-test.c
	$(CC) $(CFLAGS) -c -o $@ $<

pfq-test: $(PFQ_OBJS)
	$(CC) $(CFLAGS) -o $@ $(PFQ_OBJS) -pthread -lm

pfq-test.o: pfq-test.c
	$(CC) $(CFLAGS) -c -o $@ $<

mcs-lock.o: ../mcs-lock.c
	$(CC) $(CFLAGS) -c -o $@ $<

pfq-rwlock.o: ../pfq-rwlock.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o lock-test pfq-test
