# the Makefile that holds everything together

# work around irritating bug in GNU make--targets defined in the include
# files should not be the default target

install_loc := /home/mfagan/csprof-install/libexec

PLATFORM := x86-64
#CC :=  cc
#CXX := CC
CC   := gcc
CXX  := g++
link := gcc
static_link_options := --verbose -nostdlib -nodefaultlibs -nostartfiles -static -Wl,-r
#CHGROOT := 1 # undef for std root version

#STATIC_ONLY := 1
#NO_MMAP := 1
#USE_TRAMP := 1

src_dir_prefix = $(patsubst %Makefile,%,  \
                    $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

src_dir        = $(patsubst %/,%,$(src_dir_prefix))
export PLATFORM
export CC
#export CHGROOT
DO_THREADS := 1
PERF := -DCSPROF_PERF=1
export PERF

PRIM_UNWIND := 1
export PRIM_UNWIND

# improved unwind here
UNWIND := $(src_dir_prefix)BetterUnwind

#special decls for xed library usage
XEDDCL := -DXED_SCALARS

#MPI_SPECIAL :=1

CSPROFLIB := libcsprof.so.1
#CSPROFLIB_STATIC := libcsprof.a
CSPROF_DOT_O := libcsprof.o

#DRIVER ?= mallocprof
# MWF wants async timing, so chg default
DRIVER = itimer
DRIVER_DIR = itimer
#DRIVER_DIR = papi
#DRIVER = papi_pulse
#DRIVER = mallocprof

all: $(CSPROFLIB)
#all: $(CSPROF_DOT_O)

# we develop on Tru64, make it the default
# MWF: working on x86-64, make it (and gcc) the default 30jan07
#PLATFORM ?= ia64

#include mkinclude.$(PLATFORM)

DEFINES := -D_GNU_SOURCE
DEFINES += -DCSPROF_LAST_EDGE_ONLY=1
DEFINES += $(PERF)
#DEFINES += -I/ccs/home/johnmc/mypapi/pkgs/include

#CSPROF_TRAMPOLINE := -DCSPROF_TRAMPOLINE_BACKEND=1

CFLAGS := -fPIC -g -Wall 
#CFLAGS := -g -Wall  #CATAMOUNT

AR := /usr/bin/ar
ARFLAGS := -r

#ADA link flags
link_flags := -Wl,-Bstatic -Wl,-soname,$(CSPROFLIB) -shared
# andre1 link flags
link_flags := -Wl,-soname,$(CSPROFLIB) -shared

module_dirs := utilities                \
               memory                   \
               processor/x86-64         \
               sampling/types/callstack \
               BetterUnwind             \
               Pmsg                     \
               hpcfile                  \
               os/catamount             \
               sampling/sources/$(DRIVER_DIR)

#LIB_CSFILE := libcsfile.a
#INC_CSFILE := -I./hpcfile

ifdef STATIC_ONLY
  INC_MON := -I$(src_dir_prefix)monitor-static/src
  find_v  := find_static
  gethost := gethostid.o override.o MPI_Init.o 
else
  INC_MON := -I$(src_dir_prefix)monitor/Install/include
  find_v  := find
endif
# INC_BETTER_UNW := -IBetterUnwind

#MPI_INC := -I/usr/mpich/mpich-1.2.6-pgi614/include

ifeq "$(strip $(src_dir_prefix))" ""
src_dir_inc := .
else
src_dir_inc := $(patsubst %/,%,$(src_dir_prefix))
endif

INCS := -I$(src_dir_inc) $(INC_PFM) $(INC_UNWIND) $(INC_THREAD)\
         $(INC_MON) $(INC_BETTER_UNW)
ifdef MPI_SPECIAL
INCS += $(MPI_INC)
endif

INCS += $(addprefix -I$(src_dir_prefix),$(module_dirs))
INCS += -I$(src_dir_prefix)BetterUnwind/Include

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
OBJS += name.o list.o libc.o epoch.o

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

OBJS += swizzle.o unsafe.o
OBJS += loadmod.o processor-libc.o

ifdef USE_TRAMP
  OBJS += tramp.o 
endif

OBJS += $(DRIVER).o
OBJS += state.o env.o data.o dump_backtraces.o segv_handler.o
OBJS += pmsg.o
OBJS += nm_bound.o
OBJS += bad_unwind.o
OBJS += $(hpcfile_objs)
ifdef PRIM_UNWIND
OBJS += $(find_v).o xed_if.o
OBJS += stack_troll.o
OBJS += $(gethost)
endif
OBJS += last.o
OBJS += last_func.o

#LIBS := hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_UNWIND) $(LIB_PFM)

#LIBS := hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_PFM)
LIBS := $(LIB_THREAD) $(LIB_PFM)

# when libxed.a is used
#
XED_LIBS := -L$(UNWIND)/Lib -lxed
ifdef PRIM_UNWIND
#XED_LIBS := -LXed0-Alt/Lib -lxed
XED_CPP_LIBS :=  -lstdc++ -ldl
else
XED_LIBS := -LXed0-Alt/Lib -lxed
#XED_LIBS := -L`pwd` lxed.so
XED_CPP_LIBS :=  -lstdc++ -ldl
endif
#PAPI_LIBS := -L/ccs/home/johnmc/mypapi/pkgs/lib -lpapi -lpfm
#PAPI_LIBS  := -lpapi -lpfm

LIBS += $(XED_LIBS) $(XED_CPP_LIBS) $(PAPI_LIBS)

#############################################################################
# Compiling Rules:
#############################################################################

vpath %.c $(src_dir) $(addprefix $(src_dir_prefix),$(module_dirs))

vpath %.s $(src_dir_prefix)processor/x86-64

vpath %.cpp $(addprefix $(src_dir_prefix),$(module_dirs))

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
	# src dir prefix = |$(src_dir_prefix)|, src dir = |$(src_dir)|
	# PRIM_UNWIND = |$(PRIM_UNWIND)|
	# XED_LIBS = |$(XED_LIBS)|

$(LIB_CSFILE):
	@cd hpcfile; $(MAKE) -I ..
#	@cd BetterUnwind; $(MAKE)

clean:
	@echo "*** Cleaning csprof ***"
	@/bin/rm -f $(CSPROFLIB) $(CSPROF_DOT_O) $(OBJS)
	@cd hpcfile; $(MAKE) -I .. clean
	@cd BetterUnwind; $(MAKE) clean

.PHONY: TAGS
TAGS:
	etags *.[csh] hpcfile/*.[csh] $(addsuffix /*.[csh],$(module_dirs)) BetterUnwind/*.cpp BetterUnwind/*.[ch]

tarball: 
	@tar -cf csprof.tar $(filter-out %.o %.csp %.log t1 t2 t3,$(wildcard *))
	@gzip --best csprof.tar

.PHONY : all libcsfile clean tarball static check install

$(CSPROFLIB): $(LIB_CSFILE) $(OBJS)
#	$(link) $(link_flags) -o $@ markfront.o $(OBJS) $(LIBS) markrear.o
	$(link) $(link_flags) -o $@ $(OBJS) $(LIBS)
	@echo "*** $(CSPROFLIB) is up to date. ***"


$(CSPROF_DOT_O): $(OBJS)
	$(link) $(static_link_options) -o $@ $(OBJS) $(LIBS)
	@echo "*** $@ is up to date. ***"

.PHONY: bclean
bclean:
	rm -rf *.o *.so *.so.[0-9]*

install: all
	cp $(CSPROFLIB) $(install_loc)

%.o: %.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

%.E: %.c
	$(CC) $(INCS) $(CFLAGS) -E $< > $@

%.o: %.s
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(INCS) $(XEDDCL) $(CFLAGS) -c $< -o $@
