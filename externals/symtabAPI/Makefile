#
#  SymtabAPI Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = symtabAPI
SRC_DIR =  symtabAPI
PREREQS = libdwarf libelf libunwind libxml2

PLATFORM := $(SYMTABAPI_PLATFORM)
export PLATFORM

USE_SYMTAB_7 = yes

ifeq ($(USE_SYMTAB_7),yes)

FETCH_TARFILE = symtab_src_7.0.1.tar.gz
BUILD_DIR = symtabAPI/src/dyninst
PATCH_DIR = patches

CONFIG_CMD = ./configure
INSTALL_CMD = true

else

FETCH_TARFILE = symtab_src_6.1.tar.gz
BUILD_DIR = symtabAPI/src/dyninst
PATCH_DIR = patches-61

CONFIG_CMD = ./configure-61
INSTALL_CMD = true

endif

include ../build-utils/Makefile.inc
