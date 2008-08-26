#
#  Old Monitor Makefile.
#
#  For some reason, make -C doesn't seem to work here, so use cd
#  instead.
#
#  $Id$
#

PKG_NAME = old-monitor
SRC_DIR = monitor-080825
TEMP_INSTALL := $(shell pwd)/$(SRC_DIR)/temp-install

FETCH_CMD = http://www.cs.rice.edu/~krentel/hpc-distfiles/monitor-080825.tar.gz

CONFIG_CMD =  true
BUILD_CMD =   cd $(SRC_DIR) && $(MAKE) PREFIX=$(TEMP_INSTALL) install
INSTALL_CMD = ./install
CLEAN_CMD =   cd $(SRC_DIR) && $(MAKE) clean

include ../build-utils/Makefile.inc

