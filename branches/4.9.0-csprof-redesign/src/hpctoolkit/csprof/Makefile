# the Makefile that holds everything together

# work around irritating bug in GNU make--targets defined in the include
# files should not be the default target

PLATFORM := x86-64
CC := gcc
#CHGROOT := 1 # undef for std root version

#STATIC_ONLY := 1
#NO_MMAP := 1

export PLATFORM
export CC
#export CHGROOT
DO_THREADS := 1
#PERF := -DCSPROF_PERF=1
export PERF

PRIM_UNWIND := 1
export PRIM_UNWIND

# improved unwind here
UNWIND := BetterUnwind

#special decls for xed library usage
XEDDCL := -DXED_SCALARS

#MPI_SPECIAL :=1

CSPROFLIB := libcsprof.so.1
CSPROFLIB_STATIC := libcsprof.a

#DRIVER ?= mallocprof
# MWF wants async timing, so chg default
DRIVER = itimer
#DRIVER = mallocprof

all: $(CSPROFLIB)

# we develop on Tru64, make it the default
# MWF: working on x86-64, make it (and gcc) the default 30jan07
#PLATFORM ?= ia64

include mkinclude.$(PLATFORM)

module_dirs := utilities                \
               memory                   \
               processor/x86-64         \
               sampling/types/callstack \
               BetterUnwind             \
               sampling/sources/itimer

LIB_CSFILE := libcsfile.a
INC_CSFILE := -I./hpcfile

INC_MON := -I./monitor/src
# INC_BETTER_UNW := -IBetterUnwind

MPI_INC := -I/usr/mpich/mpich-1.2.6-pgi614/include

INCS := -I. $(INC_CSFILE) $(INC_PFM) $(INC_UNWIND) $(INC_THREAD)\
         $(INC_MON) $(INC_BETTER_UNW)
ifdef MPI_SPECIAL
INCS += $(MPI_INC)
endif

INCS += $(addprefix -I,$(module_dirs))
INCS += -IBetterUnwind/Include

# FIXME: PAPI?
ifneq "$(DRIVER)" "itimer"
DEFINES += -DCSPROF_SYNCHRONOUS_PROFILING_ONLY=1
endif
ifeq "$(DRIVER)" "mallocprof"
DEFINES += -DCSPROF_MALLOC_PROFILING=1
endif

ifdef DO_THREADS
  CSPROF_THREADS := -DCSPROF_THREADS=1
  LIB_THREAD := -lpthread
endif

ifdef PRIM_UNWIND
   DEFINES += -DPRIM_UNWIND=1
endif

ifdef NO_MMAP
   DEFINES += -DNO_MMAP=1
endif

ifdef STATIC_ONLY
DEFINES += -DSTATIC_ONLY=1
endif

DEFINES += -DINLINE_FN=1

ifdef MPI_SPECIAL
DEFINES += -DMPI_SPECIAL=1
endif

# includes might have set up their own special CFLAGS, so we add
CFLAGS += $(DEFINES) $(CSPROF_THREADS) $(CSPROF_TRAMPOLINE) -I./$(ARCH)

# do not move "csprof_first_func.o" or "csprof_last_func.o"; they need
# to be in those places to force the linker to stitch together the
# library in the proper order and for correct functioning of the profiler
OBJS := first_func.o
ifndef STATIC_ONLY
   OBJS += mon_ovr.o
endif
OBJS += csproflib.o metrics.o mem.o cct.o
OBJS += list.o libc.o epoch.o

ifdef DO_THREADS
   OBJS += pthread.o
endif

OBJS += backtrace.o
ifdef PRIM_UNWIND
  // OBJS += splay_if.o
  OBJS += splay.o
  OBJS += simple-lock.o
  OBJS += prim_unw.o
endif

OBJS += swizzle.o unsafe.o
OBJS += tramp.o loadmod.o processor-libc.o

OBJS += $(DRIVER).o
OBJS += state.o env.o data.o dump_backtraces.o segv_handler.o
OBJS += pmsg.o
OBJS += nm_bound.o
OBJS += bad_unwind.o
//OBJS += $(UNWIND)/find.o $(UNWIND)/xed_if.o
OBJS += find.o xed_if.o
OBJS += last_func.o

#LIBS := hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_UNWIND) $(LIB_PFM)

LIBS := hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_PFM)

XED_LIBS := -L$(UNWIND)/Lib -lxed
XED_CPP_LIBS :=  -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0 -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -lstdc++ -ldl

LIBS += $(XED_LIBS) $(XED_CPP_LIBS)

#############################################################################
# Compiling Rules:
#############################################################################

vpath %.c $(module_dirs)

vpath %.s processor/x86-64

vpath %.cpp $(module_dirs)

.PHONY: TT

TT:
	# OBJS = $(OBJS)
	# INCS = $(INCS)
	# CXX  = $(CXX)
	# INC_CSFILE = $(INC_CSFILE)

$(LIB_CSFILE):
	@cd hpcfile; $(MAKE) -I ..
#	@cd BetterUnwind; $(MAKE)

clean:
	@echo "*** Cleaning csprof ***"
	@/bin/rm -f $(CSPROFLIB) $(CSPROFLIB_STATIC) $(OBJS)
	@cd hpcfile; $(MAKE) -I .. clean
	@cd BetterUnwind; $(MAKE) clean

TAGS:
	@etags *.[csh] hpcfile/*.[csh] $(addsuffix /*.[csh],$(module_dirs)) BetterUnwind/*.cpp BetterUnwind/*.[ch]

tarball: 
	@tar -cf csprof.tar $(filter-out %.o %.csp %.log t1 t2 t3,$(wildcard *))
	@gzip --best csprof.tar

.PHONY : all libcsfile clean tarball static check TAGS

$(CSPROFLIB): $(LIB_CSFILE) $(OBJS)
	$(CC) $(LINK_FLAGS) -o $(CSPROFLIB) $(OBJS) $(LIBS)
	mv hpcfile/$(LIB_CSFILE) ./$(CSPROFLIB_STATIC)
	$(AR) $(ARFLAGS) $(CSPROFLIB_STATIC) $(OBJS)
	@echo "*** $(CSPROFLIB) is up to date. ***"

%.o: %.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

%.E: %.c
	$(CC) $(INCS) $(CFLAGS) -E $< > $@

%.o: %.s
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(INCS) $(XEDDCL) $(CFLAGS) -c $< -o $@

