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

FETCH_CMD = http://www.cs.rice.edu/~krentel/hpc-distfiles/binutils-2.17-hpc-r101.tar.gz

CONFIG_CMD =  $(MAKE) -C $(SRC_DIR) configure
BUILD_CMD =   $(MAKE) -C $(SRC_DIR) build install
INSTALL_CMD = ./install

include ../build-utils/Makefile.inc

