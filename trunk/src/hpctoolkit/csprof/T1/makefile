CXED    := g++
INC_PMSG := -I..
XEDINC := -I./Include
XEDDCL := -DXED_SCALARS
XEDLIB := -L../BetterUnwind/Lib -lxed
CC     := gcc
CXX    := g++
FC     := pathf90

CFLAGS := -g -I..


test_targs := 

ifdef NO
.PHONY: f2
f2: f2.f90
	$(HPCL) -v ftn -v $< -o $@

.PHONY: t1
t1: t1.o $(test_targs)
#	$(HPCL) -v $(CC) -v $(filter-out $(test_targs),$^) -o $@
	$(CC) -g $^ -lm -o $@
 
.PHONY: $(CSPROF_DOT_O)
$(CSPROF_DOT_O):
	cd ..; $(MAKE)
endif

.PHONY: tt
tt:
	ls -l $(monitor)
	cc --version

.PHONY: t-define
t-define:
	cc -o $@ t-define.c

.PHONY: clean components

tmpi: tmpi.c
	mpicc $< -lm -o $@

t1: t1.o
	$(CC) -g $< -lm -o $@

fib: fib.o
	$(CC) -g $< -o $@

tp2: tp2.o
	$(CC) -g $< -lpthread -o $@

ot1: t1.o $(CSPROF_DOT_O) 
	$(HPCL) -v CC -static -v $^ $(XEDLIB) -o $@

#$(HPCL) -v g++ -static -v $^ $(XEDLIB) -o $@


clean:
	rm -rf *.o t1 tp *.nm.c *.nm.so

%   :  %.c
	$(CC) -g $< -lm -lpthread -o $@

%   : %.f90
	$(FC) -g $< -o $@

%.o : %.f90
	$(FC) -g -c $< -o $@

%.o : %.c
	$(CC) -g -I.. -c $< -o $@

vpath %.c ..

%.E : %.c
	$(CC) -I.. -E $< > $@

dlopen-test: dlopen-test.c
	gcc -o dlopen-test -g dlopen-test.c -ldl
