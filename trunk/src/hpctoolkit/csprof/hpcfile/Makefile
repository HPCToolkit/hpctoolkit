# $Id$
#############################################################################
# Makefile
#############################################################################

all: lib

include mkinclude.$(PLATFORM)

LIBRARY = libcsfile.a

OBJS = hpcfile_general.o hpcfile_cstree.o hpcfile_csprof.o

# the library needs to know about the trampoline
CFLAGS += $(CSPROF_TRAMPOLINE)

#############################################################################
# Compiling Rules:
#############################################################################

lib: $(LIBRARY)

$(LIBRARY): $(OBJS)
	$(AR) $(ARFLAGS) $(LIBRARY) $(OBJS)
	@echo "*** $(LIBRARY) is up to date. ***"

clean:
	@echo "*** Cleaning $(LIBRARY) ***"
	@/bin/rm -f $(LIBRARY) $(OBJS)

.PHONY : all lib clean

#############################################################################

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
