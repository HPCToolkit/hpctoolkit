#
#  SymtabAPI Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = symtabAPI

PREREQS = boost libelf libdwarf

PLATFORM := $(SYMTABAPI_PLATFORM)
export PLATFORM

ifeq ($(USE_SYMTAB_82),yes)

# New symtabAPI 8.2 with inline support.

FETCH_TARFILE = dyninst-8.2.tar.gz
SRC_DIR = dyninst-8.2
PATCH_DIR = patches-82
POST_FETCH_CMD = ./post-fetch-82
CONFIG_CMD = ./configure-82
BUILD_CMD =  ./build-82
INSTALL_CMD = ./install-82
PREREQS += binutils

else
ifeq ($(USE_SYMTAB_61),yes)

# SymtabAPI dropped support for ia64 after 6.1.

FETCH_TARFILE = symtab_src_6.1.tar.gz
SRC_DIR =   symtabAPI
BUILD_DIR = symtabAPI/src/dyninst
PATCH_DIR = patches-61
CONFIG_CMD = ./configure-61
INSTALL_CMD = $(BUILD_CMD)
PREREQS += libunwind libxml2

else

FETCH_TARFILE = symtabAPI-8.0.tgz
SRC_DIR =   SymtabAPI-8.0
BUILD_DIR = SymtabAPI-8.0
PATCH_DIR = patches
POST_FETCH_CMD = ./post-fetch
CONFIG_CMD = ./configure
PREREQS += libxml2

endif
endif

include ../build-utils/Makefile.inc
