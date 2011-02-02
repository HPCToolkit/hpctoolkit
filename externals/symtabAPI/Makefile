#
#  SymtabAPI Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = symtabAPI
SRC_DIR =  symtabAPI
PREREQS = binutils libdwarf libelf libunwind libxml2

USE_SYMTAB_61 = no

ifeq ($(USE_SYMTAB_61),yes)

FETCH_TARFILE = symtab_src_6.1.tar.gz
BUILD_DIR = symtabAPI/src/dyninst
PATCH_DIR = patches-61

CONFIG_CMD = ./configure-61
INSTALL_CMD = true

else

FETCH_TARFILE = symtab_src_2.1.tar.gz
BUILD_DIR = symtabAPI/src/core

DYNINST_ROOT := $(shell pwd)/symtabAPI/src
PLATFORM := $(SYMTABAPI_PLATFORM)
export DYNINST_ROOT PLATFORM

INSTALL_CMD = ./install

endif

include ../build-utils/Makefile.inc
