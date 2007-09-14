LIBUNWIND_HOME := /home/johnmc/pkgs
#LIBUNWIND_HOME := /usr

LIB_UNWIND := -L$(LIBUNWIND_HOME)/lib -lunwind-x86_64 -lunwind -Wl,--rpath -Wl,$(LIBUNWIND_HOME)/lib -ldl
INC_UNWIND := -I$(LIBUNWIND_HOME)/include

CC := gcc
F90 := pgf90
HPCF90 := PATH=Static:$$PATH;$(F90)
HPCC   := PATH=Static:$$PATH;$(CC)
F90FLAGS-g := -g
F90FLAGS-o := -g -fast -gopt
F90FLAGS   := $(F90FLAGS-g) -v
FLDFLAGS   := -Wl,--eh-frame-hdr
LDFLAGS    := -lm -lpthread -Wl,--eh-frame-hdr
CFLAGS := -g $(INC_UNWIND) -I.. -I../monitor/src
CLDFLAGS := $(LIB_UNWIND) -lm -lpthread
HPCLDFLAGS := -Bstatic $(filter-out -ldl,-L.. -lcsprof $(LIB_UNWIND) -lbfd -liberty)
F90MAIN    := /opt/apps/pgi/7.0.3/linux86-64/7.0-3/lib/f90main.o
#F90LD      := -rpath /opt/apps/pgi/7.0.3/linux86-64/7.0-3/lib 
F90LD      := -L/opt/apps/pgi/7.0.3/linux86-64/7.0-3/lib 
F90LD2     := -lpgf90 -lpgf90_rpm1 -lpgf902 -lpgf90rtl -lpgftnrtl -lnspgc -lpgc -lrt
MPICC      := mpicc

### Try out override thing
###
#SHELL   := PATH=Static:$$PATH;$(SHELL)

.PHONY: TARGET TARGET2 TMSG T C 

all: tmpi prof.so

#tmpi: tmpi.c libcsp.so  # when making with csp thing
tmpi: tmpi.c
	$(MPICC) $(CFLAGS) $^ $(LDFLAGS) -o $@

prof.so: prof.c
	$(CC) -fPIC $(CFLAGS) -shared -o $@ -nostartfiles $< -lpthread

libcsp.so: csp.c
	$(CC) $(CFLAGS) -fPIC $< -shared -nostartfiles -o $@

cf2: cf2.o f2m.o
	$(CC) $^ -o $@  

f2: f2.o
#	$(CC) $(F90MAIN) $(F90LD) $^ -o $@ $(F90LD2)
	$(F90) -Wl,--eh-frame-hdr $^ -o $@

t4:  t4.c
	$(CC) $(CFLAGS) -lm -o $@ $<

f2.1 : f2.1.f90 hpc_main.o
	$(HPCF90) $(F90FLAGS) $^ $(HPCLDFLAGS) -o $@

f2.2 : f2.2.f90 hpc_init.o
	$(F90) $(F90FLAGS) $^ $(HPCLDFLAGS) -o $@

TMSG: mon_ovr.so tmsg

mon_ovr.o: mon_ovr.c
	$(CC) -fPIC $(CFLAGS) -c $<

msg.o: msg.c
	$(CC) -fPIC $(CFLAGS) -c $<

mon_ovr.so: mon_ovr.o msg.o
	$(CC) $(CFLAGS) -shared -nostartfiles -o $@ $^

tmsg: tmsg.o mon_ovr.so
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

tmsg.o: tmsg.c
	$(CC) -I.. -I../src/monitor -c $< 

#f2: f2.f90
	$(F90) -v -Wl,--eh-frame-hdr $(F90FLAGS) $^ -o $@

TARGET2: f1-g f1-o

TARGET: f1 f0 lib-override.so

f1-o: f1.f90
	$(F90) $(F90FLAGS-o) -o $@ $^

f1-g: f1.f90
	$(F90) $(F90FLAGS-g) -o $@ $^

f1: f1.f90
	$(F90) $(F90FLAGS) $^ $(LDFLAGS) -o $@

f0: f0.f90 f_unwind.o f_loadmod.o
	$(F90) $(F90FLAGS) $^ $(LDFLAGS) -o $@

lib-override.o: lib-override.c
	$(CC) -fPIC $(CFLAGS) -c $<

lib-override.so: lib-override.o f_unwind.o sighandlers.o timers.o
	$(CC) $(LIB_UNWIND) -shared -nostartfiles -o $@ $^ 

sighandlers.o : sighandlers.c
	$(CC) -fPIC $(CFLAGS) -c $<

timers.o : timers.c
	$(CC) -fPIC $(CFLAGS) -c $<

f_unwind.o: f_unwind.c
	$(CC) -fPIC $(CFLAGS) -c $<

tp: tp.c 
tp0: tp0.c loadmod.o segv_handler.o
loadmod.o: loadmod.c utils.h

f_loadmod.o: f_loadmod.c
g:  g.c main.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@


tp0.E: tp0.c
	$(CC) -E $(CFLAGS) $^ > $@

%.o : %.f90
	$(F90) $(F90FLAGS) -c $<

%: %.f90
	$(F90) $(F90FLAGS) -o $@ $<

t1: t1.c
t2: t2.c
t3: t3.c

clean:
	rm -rf *.o t[123] f[0-9] *.so f1-[go] f2.1

T:
	@echo which ld = `which ld`

C:
	rm -rf tmpi tmpi.o prof.so

