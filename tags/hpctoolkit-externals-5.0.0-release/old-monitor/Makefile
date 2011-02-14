#
#  Old Monitor Makefile.
#
#  For some reason, make -C doesn't seem to work here, so use cd
#  instead.  Also, the build fails with some non-gcc compilers, so
#  force it to use gcc.
#
#  $Id$
#

PKG_NAME = old-monitor
SRC_DIR = monitor-080825
TEMP_INSTALL := $(shell pwd)/$(SRC_DIR)/temp-install

# FIXME: tallent: dangerous (see libdwarf/configure) but I have a work-around
override CC = gcc
override CFLAGS = -g -O2

FETCH_TARFILE = monitor-080825.tar.gz

BUILD_CMD =   cd $(SRC_DIR) && $(MAKE) PREFIX=$(TEMP_INSTALL) install
INSTALL_CMD = ./install
CLEAN_CMD =   cd $(SRC_DIR) && $(MAKE) clean

include ../build-utils/Makefile.inc

