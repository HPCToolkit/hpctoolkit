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

PLATFORM := $(SYMTABAPI_PLATFORM)
export PLATFORM

# SymtabAPI dropped support for ia64 after 6.1.

INSTALL_CMD = ./install

ifeq ($(USE_SYMTAB_61),yes)

FETCH_TARFILE = symtab_src_6.1.tar.gz
PATCH_DIR = patches-61
CONFIG_CMD = ./configure-61

else

FETCH_TARFILE = symtab_src_7.0.1.tar.gz
PATCH_DIR = patches
CONFIG_CMD = ./configure

endif

include ../build-utils/Makefile.inc
