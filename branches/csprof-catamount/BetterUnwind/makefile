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

LPATH= -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0 -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -lstdc++ -ldl # -lm -lgcc_s -lgcc -lc -lgcc_s -lgcc  -ldl -lgcc_s


.PHONY: clean components

components: xed_if.o find.o

xed_if.o: xed_if.cpp
	$(CXED) -fPIC -I.. $(XEDDCL) -g $(XEDINC) -c $<

#	$(CXED) $^ $(XEDLIB) -o $@
tprog: tprog.o xed_if.o find.o
	mpiCC -v -Wl,--eh-frame-hdr -g $^ $(XEDLIB) -o $@ $(LPATH)

find.o: find.cpp
	$(CXED) -fPIC -I. $(INC_PMSG) -g -c $<

clean:
	rm -rf *.o tprog
