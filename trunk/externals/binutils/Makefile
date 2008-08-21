#
#  Binutils Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = binutils
SRC_DIR = version-binutils-2.17
HPC_PLATFORM := $(shell cd $(SRC_DIR)/config && ./hpcplatform)

export CC
export CFLAGS

FETCH_CMD = svn co http://hpc.svn.rice.edu/r/binutils/tags/version-binutils-2.17

CONFIG_CMD =  $(MAKE) -C $(SRC_DIR) configure
BUILD_CMD =   $(MAKE) -C $(SRC_DIR) build install
INSTALL_CMD = ./install
CLEAN_CMD =   $(MAKE) -C $(SRC_DIR)/build-$(HPC_PLATFORM) clean

include ../build-utils/Makefile.inc

