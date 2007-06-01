LIBUNWIND_HOME := /home/johnmc/pkgs
#LIBUNWIND_HOME := /usr

LIB_UNWIND := -L$(LIBUNWIND_HOME)/lib -lunwind-x86_64 -lunwind -Wl,--rpath -Wl,$(LIBUNWIND_HOME)/lib -ldl
INC_UNWIND := -I$(LIBUNWIND_HOME)/include

CC := gcc
F90 := pgf90
F90FLAGS-g := -O -g
F90FLAGS-o := -g -fast -gopt
F90FLAGS   := $(F90FLAGS-g)
CFLAGS := -g $(INC_UNWIND)
LDFLAGS := $(LIB_UNWIND) -lm -lpthread

.PHONY: TARGET TARGET2

f2: f2.f90
	$(F90) $(F90FLAGS) $^ $(LDFLAGS) -o $@

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

t1: t1.c
t2: t2.c
t3: t3.c

clean:
	rm -rf *.o t[123] f0 f1 *.so f1-[go]
