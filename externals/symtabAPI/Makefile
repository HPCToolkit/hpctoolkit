#
#  SymtabAPI Makefile.
#
#  $Id: Makefile 1568 2008-08-30 19:46:47Z krentel $
#
include ../Makefile.conf

PKG_NAME = symtabAPI
SRC_DIR =  symtabAPI
BUILD_DIR = symtabAPI/src/core

PREREQS = libdwarf libelf libxml2

DYNINST_ROOT := $(shell pwd)/symtabAPI/src
PLATFORM := $(SYMTABAPI_PLATFORM)
export DYNINST_ROOT PLATFORM

FETCH_TARFILE = symtab_src_2.1.tar.gz
FETCH_URL_1 = ftp://ftp.cs.wisc.edu/paradyn/releases/release5.2/$(FETCH_TARFILE)

INSTALL_CMD = ./install

include ../build-utils/Makefile.inc
