#
#  Xed2 Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = xed2

ifeq ($(USE_XED2_K1OM),yes)
FETCH_TARFILE = pin-extras-2.12-58423-k1om.tar.gz
SRC_DIR = extras

else ifeq ($(USE_XED2_MIC),yes)
FETCH_TARFILE = xed-install-knc-2013-05-31-lin-x86-64.tar.bz2
SRC_DIR = xed-install-knc-2013-05-31-lin-x86-64

else
FETCH_TARFILE = pin-extras-2.12-53271-x86-linux.tar.gz
SRC_DIR = extras
endif

CONFIG_CMD = true
BUILD_CMD = true
INSTALL_CMD = ./install

include ../build-utils/Makefile.inc
