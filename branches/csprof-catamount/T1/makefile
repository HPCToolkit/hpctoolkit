#CXED   := g++-4.1.2
#CXED   := g++
#CXED    := CC -target=catamount
CXED    := g++
INC_PMSG := -I..
XEDINC := -I./Include
XEDDCL := -DXED_SCALARS
XEDLIB := -L../BetterUnwind/Lib -lxed
CC     := gcc
CXX    := g++
FC     := pgf90
#CC      := cc
#CXX     := CC

INSTALL := ~/xxx-csprof/csprof-catamount-install

CFLAGS := -g -I..
#CLINK    := mpicc  
HPCL     := ../Scripts+Tools/hpc-link/hpc-link.sh

monitor  := $(INSTALL)/lib
CSPROF_DOT_O := ../libcsprof.o

LPATH = -m elf_x86_64 -static -L/opt/xt-mpt/1.5.52/mpich2-64/GP2/lib -L/opt/acml/3.6/gnu64/lib -L/opt/xt-libsci/1.5.52/gnu/cnos64/lib -L/opt/xt-mpt/1.5.52/sma/P2/lib -L/opt/xt-tools/iobuf/1.0.5/lib/cnos64 -L/opt/xt-lustre-ss/1.5.52/lib64 -L/opt/xt-catamount/1.5.52/lib/cnos64 -L/opt/xt-libc/1.5.52/amd64/lib -L/opt/xt-pe/1.5.52/lib/cnos64 -L/opt/xt-os/1.5.52/lib/cnos64 -L/opt/xt-service/1.5.52/lib/cnos64 -L/opt/gcc/4.2.0/cnos/lib/gcc-lib/x86_64-suse-linux/4.2.0/ -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0 -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -L/apps/jaguar/lib -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0/../../..  -lsma -lmpichcxx -lmpich -lstdc++ -lgcc_eh -lportals -lcatamount # -lc -lgcc -lm --end /opt/xt-pe/1.5.52/lib/cnos64/crtend.o

#test_targs := $(CSPROF_DOT_O) $(monitor)

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

t1: t1.o
	$(CC) -g $< -lm -o $@

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
