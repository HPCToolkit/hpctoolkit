# the Makefile that holds everything together

# work around irritating bug in GNU make--targets defined in the include
# files should not be the default target

PLATFORM := x86-64
CC :=  cc
CXX := CC
link := gcc
link_options := --verbose -nostdlib -nodefaultlibs -nostartfiles -static -Wl,-r
#CHGROOT := 1 # undef for std root version

STATIC_ONLY := 1
NO_MMAP := 1

export PLATFORM
export CC
#export CHGROOT
#DO_THREADS := 1
PERF := -DCSPROF_PERF=1
export PERF

PRIM_UNWIND := 1
export PRIM_UNWIND

# improved unwind here
UNWIND := BetterUnwind

#special decls for xed library usage
XEDDCL := -DXED_SCALARS

#MPI_SPECIAL :=1

CSPROFLIB := libcsprof.so.1
CSPROFLIB := foolibcsprof.a
CSPROFLIB_STATIC := libcsprof.a
CSPROF_DOT_O := libcsprof.o

#DRIVER ?= mallocprof
# MWF wants async timing, so chg default
#DRIVER = itimer
#DRIVER_DIR = itimer
DRIVER_DIR = papi
DRIVER = papi_pulse
#DRIVER = mallocprof

#all: $(CSPROFLIB)
all: $(CSPROF_DOT_O)

# we develop on Tru64, make it the default
# MWF: working on x86-64, make it (and gcc) the default 30jan07
#PLATFORM ?= ia64

#include mkinclude.$(PLATFORM)

DEFINES := -D_GNU_SOURCE
DEFINES += -DCSPROF_LAST_EDGE_ONLY=1
DEFINES += $(PERF)

#CSPROF_TRAMPOLINE := -DCSPROF_TRAMPOLINE_BACKEND=1

CFLAGS := -fPIC -g -Wall 
CFLAGS := -g -Wall  #CATAMOUNT

AR := /usr/bin/ar
ARFLAGS := -r

#LINK_FLAGS := -Wl,-soname,$(CSPROFLIB) -shared

module_dirs := utilities                \
               memory                   \
               processor/x86-64         \
               sampling/types/callstack \
               BetterUnwind             \
               hpcfile                  \
               os/catamount             \
               sampling/sources/$(DRIVER_DIR)

#LIB_CSFILE := libcsfile.a
#INC_CSFILE := -I./hpcfile

ifdef STATIC_ONLY
  INC_MON := -I./monitor-static/src
  find_v  := find_static
  gethost := gethostid.o
else
  INC_MON := -I./monitor/src
  find_v  := find
endif
# INC_BETTER_UNW := -IBetterUnwind

#MPI_INC := -I/usr/mpich/mpich-1.2.6-pgi614/include

INCS := -I. $(INC_CSFILE) $(INC_PFM) $(INC_UNWIND) $(INC_THREAD)\
         $(INC_MON) $(INC_BETTER_UNW)
ifdef MPI_SPECIAL
INCS += $(MPI_INC)
endif

INCS += $(addprefix -I,$(module_dirs))
INCS += -IBetterUnwind/Include

# FIXME: PAPI?
ifneq "$(DRIVER_DIR)" "itimer"
DEFINES += -DCSPROF_SYNCHRONOUS_PROFILING_ONLY=1
endif
ifeq "$(DRIVER_DIR)" "mallocprof"
DEFINES += -DCSPROF_MALLOC_PROFILING=1
endif
ifeq "$(DRIVER_DIR)" "papi"
INCS += $(PAPI_INCLUDE_OPTS)
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

hpcfile_objs := $(addsuffix .o,$(addprefix hpcfile_,general cstree csprof))

# do not move "csprof_first_func.o" or "csprof_last_func.o"; they need
# to be in those places to force the linker to stitch together the
# library in the proper order and for correct functioning of the profiler
OBJS := first_func.o
ifndef STATIC_ONLY
   OBJS += mon_ovr.o
else
   OBJS += static_mon_ovr.o
endif
OBJS += csproflib.o metrics.o mem.o cct.o
OBJS += list.o libc.o epoch.o

ifdef DO_THREADS
   OBJS += pthread.o
endif

OBJS += backtrace.o
ifdef PRIM_UNWIND
  #OBJS += splay_if.o
  OBJS += splay.o
  OBJS += simple-lock.o
  OBJS += prim_unw.o
endif

#OBJS += swizzle.o unsafe.o
OBJS += loadmod.o processor-libc.o

ifndef STATIC_ONLY
  OBJS += tramp.o 
endif

OBJS += $(DRIVER).o
OBJS += state.o env.o data.o dump_backtraces.o segv_handler.o
OBJS += pmsg.o
OBJS += nm_bound.o
OBJS += bad_unwind.o
OBJS += $(hpcfile_objs)
#OBJS += $(UNWIND)/find.o $(UNWIND)/xed_if.o
#OBJS += find.o xed_if.o
OBJS += $(find_v).o xed_if.o
OBJS += $(gethost)
OBJS += last_func.o

#LIBS := hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_UNWIND) $(LIB_PFM)

#LIBS := hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_PFM)
LIBS := $(LIB_THREAD) $(LIB_PFM)

XED_LIBS := -L$(UNWIND)/Lib -lxed
#XED_CPP_LIBS :=  -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0 -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc -L/opt/apps/gcc/gcc-4.1.0/bin/../lib/gcc/x86_64-unknown-linux-gnu/4.1.0/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -lstdc++ -ldl

XED_CPP_LIBS :=  -L/opt/xt-mpt/1.5.52/mpich2-64/GP2/lib -L/opt/acml/3.6/gnu64/lib -L/opt/xt-libsci/1.5.52/gnu/cnos64/lib -L/opt/xt-mpt/1.5.52/sma/P2/lib -L/opt/xt-tools/papi/3.5.0C.1/xt-cat/lib -L/opt/xt-tools/iobuf/1.0.5/lib/cnos64 -L/opt/xt-lustre-ss/1.5.52/lib64 -L/opt/xt-catamount/1.5.52/lib/cnos64 -L/opt/xt-libc/1.5.52/amd64/lib -L/opt/xt-pe/1.5.52/lib/cnos64 -L/opt/xt-os/1.5.52/lib/cnos64 -L/opt/xt-service/1.5.52/lib/cnos64 -L/opt/gcc/4.2.0/cnos/lib/gcc-lib/x86_64-suse-linux/4.2.0/ -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0 -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -L/spin/apps/Python/2.5/rizzo/python/2.5.1/cnl_gcc-3.3.3/lib -L/apps/jaguar/lib -L/opt/gcc/4.2.0/cnos/lib/gcc/x86_64-suse-linux/4.2.0/../../.. -lstdc++ -lgcc_eh

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
	# LIBS = $(LIBS)
	# CXX  = $(CXX)
	# CC   = $(CC)
	# INC_CSFILE = $(INC_CSFILE)
	$(CC) --version
	# gethostid := |$(gethostid)|

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


# $(CSPROF_DOT_O): $(LIB_CSFILE) $(OBJS)
$(CSPROF_DOT_O): $(OBJS)
#	$(CC) --verbose -nostdlib -nodefaultlibs -nostartfiles -r -o $@ $(OBJS) $(LIBS)
	$(link) $(link_options) -o $@ $(OBJS) $(LIBS)
	@echo "*** $@ is up to date. ***"

%.o: %.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

%.E: %.c
	$(CC) $(INCS) $(CFLAGS) -E $< > $@

%.o: %.s
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(INCS) $(XEDDCL) $(CFLAGS) -c $< -o $@

