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

FETCH_TARFILE = monitor-080825.tar.gz
FETCH_URL_1 = https://outreach.scidac.gov/frs/download.php/52/$(FETCH_TARFILE)

CONFIG_CMD =  true
BUILD_CMD =   cd $(SRC_DIR) && $(MAKE) PREFIX=$(TEMP_INSTALL) install
INSTALL_CMD = ./install
CLEAN_CMD =   cd $(SRC_DIR) && $(MAKE) clean

include ../build-utils/Makefile.inc

