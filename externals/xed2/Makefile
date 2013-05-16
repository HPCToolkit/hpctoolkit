#
#  Xed2 Makefile.
#
#  $Id$
#
include ../Makefile.conf

PKG_NAME = xed2
SRC_DIR = extras

ifeq ($(USE_XED2_K1OM),yes)
FETCH_TARFILE = pin-extras-2.12-58423-k1om.tar.gz
else
FETCH_TARFILE = pin-extras-2.12-53271-x86-linux.tar.gz
endif

CONFIG_CMD = true
BUILD_CMD = true
INSTALL_CMD = ./install

include ../build-utils/Makefile.inc
