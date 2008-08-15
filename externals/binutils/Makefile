#
#  Binutils Makefile.
#
#  $Id$
#

PKG_NAME = binutils
SRC_DIR = version-binutils-2.17

FETCH_CMD = svn co http://hpc.svn.rice.edu/r/binutils/tags/version-binutils-2.17

CONFIG_CMD =  $(MAKE) -C $(SRC_DIR) configure
BUILD_CMD =   $(MAKE) -C $(SRC_DIR) build install
INSTALL_CMD = ./install
CLEAN_CMD =   $(MAKE) -C $(SRC_DIR) clean

include ../build-utils/Makefile.inc

