##
##  Symbols Makefile.am.
##
##  $Id$
##

pkglibexecdir = $(libexecdir)/$(PACKAGE)

pkglibexec_PROGRAMS = hpcfnbounds-bin
pkglibexec_SCRIPTS =  hpcfnbounds

LIBDWARF_LIB = @LIBDWARF_LIB@
LIBELF_LIB = @LIBELF_LIB@
SYMTABAPI_INC = @SYMTABAPI_INC@
SYMTABAPI_LIB = @SYMTABAPI_LIB@
XED2_INC = @XED2_INC@
XED2_LIB = @XED2_LIB@

SYMBOLS_BASE_FILES = 		\
    code-ranges.cpp		\
    function-entries.cpp	\
    intervals.cpp		\
    symbols.cpp

hpcfnbounds_bin_SOURCES = $(SYMBOLS_BASE_FILES)

hpcfnbounds_bin_CPPFLAGS = -I$(SYMTABAPI_INC)

hpcfnbounds_bin_CXXFLAGS = $(CXXFLAGS) $(HOST_CXXFLAGS)

hpcfnbounds_bin_LDFLAGS = 			\
    -L$(SYMTABAPI_LIB) -lsymtabAPI -lcommon	\
    -L$(LIBDWARF_LIB) -ldwarf			\
    -L$(LIBELF_LIB) -lelf

if HOST_CPU_X86
    hpcfnbounds_bin_SOURCES  += x86-process-ranges.cpp
    hpcfnbounds_bin_CPPFLAGS += -I$(XED2_INC)
    hpcfnbounds_bin_LDFLAGS  += $(XED2_LIB)/libxed.a
else
    hpcfnbounds_bin_SOURCES += generic-process-ranges.cpp
endif

