#CXED   := g++-4.1.2
#CXED   := g++
#CXED    := CC -target=catamount
CXED    := g++
INC_PMSG := -I..
XEDINC := -I./Include
XEDDCL := -DXED_SCALARS
XEDLIB := -L./Lib -lxed
CC     := gcc
CFLAGS := -g -I..
CLINK    := mpicc  
HPCL     := ../Scripts+Tools/hpc-link/hpc-link.sh

LPATH= -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0 -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -lstdc++ -ldl # -lm -lgcc_s -lgcc -lc -lgcc_s -lgcc  -ldl -lgcc_s

LPATH = -m elf_x86_64 -static -L/opt/xt-mpt/1.5.52/mpich2-64/GP2/lib -L/opt/acml/3.6/gnu64/lib -L/opt/xt-libsci/1.5.52/gnu/cnos64/lib -L/opt/xt-mpt/1.5.52/sma/P2/lib -L/opt/xt-tools/iobuf/1.0.5/lib/cnos64 -L/opt/xt-lustre-ss/1.5.52/lib64 -L/opt/xt-catamount/1.5.52/lib/cnos64 -L/opt/xt-libc/1.5.52/amd64/lib -L/opt/xt-pe/1.5.52/lib/cnos64 -L/opt/xt-os/1.5.52/lib/cnos64 -L/opt/xt-service/1.5.52/lib/cnos64 -L/opt/gcc/4.2.0/cnos/lib/gcc-lib/x86_64-suse-linux/4.2.0/ -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0 -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -L/apps/jaguar/lib -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0/../../..  -lsma -lmpichcxx -lmpich -lstdc++ -lgcc_eh -lportals -lcatamount # -lc -lgcc -lm --end /opt/xt-pe/1.5.52/lib/cnos64/crtend.o



.PHONY: clean components

tprog: tprog.o xed_if.o find_static.o nm_bound.o pmsg.o
	$(HPCL) -v CC -static -v $^ $(XEDLIB) -o $@

tp2: tp2.o xed2.o pmsg.o simple-lock.o
#	CC -v $^ $(XEDLIB) -o $@
	g++ -v $^ $(XEDLIB) -o $@

#$(HPCL) -v g++ -static -v $^ $(XEDLIB) -o $@

#$(HPCL) -v g++ -static -v $^ $(XEDLIB) -o $@


components: xed_if.o find.o

xed_if.o: xed_if.cpp
	$(CXED) -fPIC -I.. $(XEDDCL) -g $(XEDINC) -c $<

xed2.o: xed2.cpp
	$(CXED) -fPIC -I.. $(XEDDCL) -g $(XEDINC) -c $<

#	$(CXED) $^ $(XEDLIB) -o $@
#tprog: tprog.o xed_if.o find.o
#	mpiCC -v -g $^ $(XEDLIB) -o $@ $(LPATH)

find_static.o: find_static.cpp
	$(CXED) -fPIC -I. $(INC_PMSG) -g -c $<

find.o: find.cpp
	$(CXED) -fPIC -I. $(INC_PMSG) -g -c $<

clean:
	rm -rf *.o tprog tp2

%.o : %.c
#	cc -I.. -c $< -o $@
	gcc -I. -I.. -c $< -o $@

vpath %.c ..
