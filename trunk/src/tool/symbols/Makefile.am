##
##  Symbols Makefile.am.
##
##  $Id$
##

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

libexec_PROGRAMS = stapi_syms

stapi_syms_SOURCES = $(SYMBOLS_BASE_FILES)

stapi_syms_CPPFLAGS = -I$(SYMTABAPI_INC)

stapi_syms_CXXFLAGS = $(CXXFLAGS) $(HOST_CXXFLAGS)

stapi_syms_LDFLAGS = 				\
    -L$(SYMTABAPI_LIB) -lsymtabAPI -lcommon	\
    -L$(LIBDWARF_LIB) -ldwarf			\
    -L$(LIBELF_LIB) -lelf

if HOST_CPU_X86
    stapi_syms_SOURCES  += x86-process-ranges.cpp
    stapi_syms_CPPFLAGS += -I$(XED2_INC)
    stapi_syms_LDFLAGS  += $(XED2_LIB)/libxed.a
else
    stapi_syms_SOURCES += generic-process-ranges.cpp
endif

