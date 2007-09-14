CC     := gcc
CFLAGS := -g -I..
LDFLAGS := -lm
STATIC  := 1

.PHONY: clean

ifndef STATIC
t_nm_bound: t_nm_bound.o nm_bound.so
	$(CC) $(LDFLAGS) $^ -o $@
	../nm_tbl.py $@
else
t_nm_bound: t_nm_bound.o nm_bound.o dummy.nm.o
	$(CC) $(LDFLAGS) $^ -o $@
	../nm_tbl.py $@
	rm -f $@.nm.so
	$(CC) $(CFLAGS) -c $@.nm.c
	$(CC) $(LDFLAGS) $(subst dummy.nm.o,$@.nm.o,$^) -o $@
endif


nm_bound.o: ../nm_bound.c
ifdef STATIC
	$(CC) $(CFLAGS) -c $< -o $@
else
	$(CC) -fPIC $(CFLAGS) -c $< -o $@
endif


nm_bound.so: nm_bound.o dummy.nm.so
	$(CC) -shared -o $@ -nostartfiles $^

dummy.nm.o: ../dummy.nm.c
	$(CC) $(CFLAGS) -c $<

dummy.nm.so: ../dummy.nm.c
	$(CC) -fPIC $(CFLAGS) -shared -o $@ -nostartfiles $<

t_jb: t_jb.c

t_pmsg: t_pmsg.c ../pmsg.o

% : %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

%.E : %.c
	$(CC) $(CFLAGS) -E $< > $@

clean:
	rm -rf t_nm_bound *.o *.E *.so t_jb t_pmsg *.nm.c

