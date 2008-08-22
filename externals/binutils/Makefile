#
#  Binutils Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = binutils
SRC_DIR =  version-binutils-2.17
BUILD_DIR = $(SRC_DIR)/build-$(shell cd $(SRC_DIR)/config && ./hpcplatform)

export CC
export CFLAGS

FETCH_CMD = svn co http://hpc.svn.rice.edu/r/binutils/tags/version-binutils-2.17

CONFIG_CMD =  $(MAKE) -C $(SRC_DIR) configure
BUILD_CMD =   $(MAKE) -C $(SRC_DIR) build install
INSTALL_CMD = ./install

include ../build-utils/Makefile.inc

