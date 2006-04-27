# the Makefile that holds everything together

# work around irritating bug in GNU make--targets defined in the include
# files should not be the default target
CSPROFLIB := libcsprof.so.1
CSPROFLIB_STATIC := libcsprof.a

DRIVER ?= mallocprof

all: $(CSPROFLIB)

# we develop on Tru64, make it the default
PLATFORM ?= ia64

include mkinclude.$(PLATFORM)

LIB_CSFILE := libcsfile.a
INC_CSFILE := -I./hpcfile

INCS := -I. $(INC_CSFILE) $(INC_PFM) $(INC_UNWIND) $(INC_THREAD)

# FIXME: PAPI?
ifneq "$(DRIVER)" "itimer"
DEFINES += -DCSPROF_SYNCHRONOUS_PROFILING_ONLY=1
endif

# includes might have set up their own special CFLAGS, so we add
CFLAGS += $(DEFINES) $(CSPROF_THREADS) $(CSPROF_TRAMPOLINE) -I./$(ARCH)

# do not move "csprof_first_func.o" or "csprof_last_func.o"; they need
# to be in those places to force the linker to stitch together the
# library in the proper order and for correct functioning of the profiler
OBJS := first_func.o
OBJS += csproflib.o metrics.o mem.o cct.o
OBJS += list.o libc.o epoch.o
OBJS += $(ARCH)/backtrace.o $(ARCH)/swizzle.o $(ARCH)/unsafe.o
OBJS += $(ARCH)/tramp.o $(ARCH)/loadmod.o $(ARCH)/libc.o
OBJS += $(ARCH_OBJS)
# FIXME: we don't really want to include this if we're building for
# synchronous drivers.  how to accomodate both at the same time?
OBJS += $(DRIVER).o
OBJS += state.o env.o data.o
OBJS += last_func.o

LIBS = hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_UNWIND) $(LIB_PFM)

#############################################################################
# Compiling Rules:
#############################################################################

$(LIB_CSFILE):
	@cd hpcfile; $(MAKE) -I ..

clean:
	@echo "*** Cleaning csprof ***"
	@/bin/rm -f $(CSPROFLIB) $(CSPROFLIB_STATIC) $(OBJS)
	@cd hpcfile; $(MAKE) -I .. clean

.PHONY : all libcsfile clean

$(CSPROFLIB): $(LIB_CSFILE) $(OBJS)
	$(CC) $(LINK_FLAGS) -o $(CSPROFLIB) $(OBJS) $(LIBS)
	mv hpcfile/$(LIB_CSFILE) ./$(CSPROFLIB_STATIC)
	$(AR) $(ARFLAGS) $(CSPROFLIB_STATIC) $(OBJS)
	@echo "*** $(CSPROFLIB) is up to date. ***"

%.o: %.c
	$(CC) $(INCS) $(CFLAGS) -c -o $@ $<
