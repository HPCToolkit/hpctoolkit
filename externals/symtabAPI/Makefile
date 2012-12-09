#
#  SymtabAPI Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = symtabAPI

PREREQS = boost libdwarf libelf libunwind libxml2

PLATFORM := $(SYMTABAPI_PLATFORM)
export PLATFORM

# SymtabAPI dropped support for ia64 after 6.1.

ifeq ($(USE_SYMTAB_61),yes)

FETCH_TARFILE = symtab_src_6.1.tar.gz
SRC_DIR =   symtabAPI
BUILD_DIR = symtabAPI/src/dyninst
PATCH_DIR = patches-61
CONFIG_CMD = ./configure-61
INSTALL_CMD = true

else

FETCH_TARFILE = symtabAPI-8.0.tgz
SRC_DIR =   SymtabAPI-8.0
BUILD_DIR = SymtabAPI-8.0
PATCH_DIR = patches
POST_FETCH_CMD = ./post-fetch
CONFIG_CMD = ./configure

endif

include ../build-utils/Makefile.inc
