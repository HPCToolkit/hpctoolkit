CFLAGS = -O3 -g -Wall -I.. -std=c99
MCS_OBJS = mcs-test.o mcs-lock.o

all: mcs-test

mcs-test: $(MCS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MCS_OBJS) -pthread -lm

mcs-test.o: mcs-test.c
	$(CC) $(CFLAGS) -c -o $@ $<

mcs-lock.o: ../mcs-lock.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o mcs-test
