CFLAGS = -O3 -g -Wall -I.. -I../../../../BUILD/src/include -std=c99
MCS_OBJS = mcs-test.o mcs-lock.o
PFQ_OBJS = pfq-test.o pfq-rwlock.o mcs-lock.o

all: mcs-test pfq-test

mcs-test: $(MCS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MCS_OBJS) -pthread -lm

mcs-test.o: mcs-test.c
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
	rm -f *.o mcs-test
