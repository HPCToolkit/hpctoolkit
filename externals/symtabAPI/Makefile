#
#  SymtabAPI Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = symtabAPI
SRC_DIR =  symtabAPI
BUILD_DIR = symtabAPI/src/core

PREREQS = libdwarf libelf libunwind libxml2

DYNINST_ROOT := $(shell pwd)/symtabAPI/src
PLATFORM := $(SYMTABAPI_PLATFORM)
export DYNINST_ROOT PLATFORM

FETCH_TARFILE = symtab_src_2.1.tar.gz

INSTALL_CMD = ./install

include ../build-utils/Makefile.inc
