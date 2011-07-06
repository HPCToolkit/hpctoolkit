#
#  SymtabAPI Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = symtabAPI
SRC_DIR =  symtabAPI
BUILD_DIR = symtabAPI/src/dyninst

PREREQS = libdwarf libelf libunwind libxml2

FETCH_TARFILE = symtab_src_7.0.1.tar.gz

INSTALL_CMD = true

PLATFORM := $(SYMTABAPI_PLATFORM)
export PLATFORM

include ../build-utils/Makefile.inc
