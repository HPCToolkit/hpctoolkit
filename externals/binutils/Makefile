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

FETCH_TARFILE = binutils-2.17-hpc-r102.tar.gz
FETCH_URL_1 = https://outreach.scidac.gov/frs/download.php/51/$(FETCH_TARFILE)

CONFIG_CMD =  $(MAKE) -C $(SRC_DIR) configure
BUILD_CMD =   $(MAKE) -C $(SRC_DIR) build install
INSTALL_CMD = ./install

include ../build-utils/Makefile.inc

