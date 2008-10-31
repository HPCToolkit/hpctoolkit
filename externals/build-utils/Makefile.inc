#
#  Makefile include rules.
#
#  Copyright (c) 2008, Rice University.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
#  * Neither the name of Rice University (RICE) nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
#  This software is provided by RICE and contributors "as is" and any
#  express or implied warranties, including, but not limited to, the
#  implied warranties of merchantability and fitness for a particular
#  purpose are disclaimed. In no event shall RICE or contributors be
#  liable for any direct, indirect, incidental, special, exemplary, or
#  consequential damages (including, but not limited to, procurement of
#  substitute goods or services; loss of use, data, or profits; or
#  business interruption) however caused and on any theory of liability,
#  whether in contract, strict liability, or tort (including negligence
#  or otherwise) arising in any way out of the use of this software, even
#  if advised of the possibility of such damage.
#
#  $Id$
#

FETCH_COOKIE =   .fetch-done
CONFIG_COOKIE =  .config-done
BUILD_COOKIE =   .build-done
INSTALL_COOKIE = .install-done
NOBUILD_COOKIE = .dont-build
BAK = .bak

PATCH_DIR ?=  patches
BUILD_DIR ?=  work

FETCH_CMD ?=    true
PATCH_CMD ?= 	default
CONFIG_CMD ?=   ./configure
BUILD_CMD ?=	$(MAKE) -C "$(BUILD_DIR)"
INSTALL_CMD ?=  $(MAKE) -C "$(BUILD_DIR)" install
CLEAN_CMD ?=    test ! -d "$(BUILD_DIR)" || $(MAKE) -C "$(BUILD_DIR)" clean
DISTCLEAN_CMD ?=  default

FETCH_FTP =  ../build-utils/fetch-ftp.sh

# Pass make to configure scripts.
export MAKE

.PHONY: all prereqs fetch refetch config reconfig build rebuild
.PHONY: install reinstall clean distclean

all: build

prereqs:
	@for pkg in $(PREREQS) ; do  \
		$(MAKE) -C "../$$pkg" install || exit 2 ;  \
	done

refetch:
	@echo ; echo "===> ($(PKG_NAME)) make fetch ..."
	@test x != "x$(SRC_DIR)"
	@if test -d "$(SRC_DIR)$(BAK)" ; then  \
		echo rm -rf "$(SRC_DIR)$(BAK)" ;  \
		rm -rf "$(SRC_DIR)$(BAK)" ;  \
	fi
	@if test -d "$(SRC_DIR)" ; then  \
		echo mv -f "$(SRC_DIR)" "$(SRC_DIR)$(BAK)" ;  \
		mv -f "$(SRC_DIR)" "$(SRC_DIR)$(BAK)" ;  \
	fi
	@rm -f $(FETCH_COOKIE) $(CONFIG_COOKIE)
	@rm -f $(BUILD_COOKIE) $(INSTALL_COOKIE)
	@if test "x$(FETCH_TARFILE)" != x ; then  \
		$(FETCH_FTP)  \
			"$(FETCH_TARFILE)"  \
			"$(FETCH_URL_1)"    \
			"$(FETCH_URL_2)"    \
			"$(FETCH_URL_3)"    \
			"$(FETCH_URL_4)" ;  \
	elif test "x$(FETCH_CMD)" != x ; then  \
		echo "$(FETCH_CMD)" ;  \
		$(FETCH_CMD) ;  \
	else  \
		echo "missing FETCH_CMD" ;  \
		exit 2 ;  \
	fi
	@if test "x$(PATCH_CMD)" != xdefault ; then  \
		echo ; echo "===> ($(PKG_NAME)) patching ..." ;  \
		$(PATCH_CMD) ;  \
	elif test -d "$(PATCH_DIR)" ; then  \
		echo ; echo "===> ($(PKG_NAME)) patching ..." ;  \
		for p in $(PATCH_DIR)/patch-* ; do  \
			if test -f "$$p" ; then  \
				echo `basename $$p` ;  \
				patch -d "$(SRC_DIR)" -p0 <"$$p" || exit 2 ;  \
			fi ;  \
		done ;  \
	fi
	touch $(FETCH_COOKIE)

fetch:
	@if test -f $(NOBUILD_COOKIE) ; then  \
		echo "$(PKG_NAME): `cat $(NOBUILD_COOKIE)`" ;  \
	elif test ! -f $(FETCH_COOKIE) ; then  \
		$(MAKE) refetch ;  \
	fi

reconfig: fetch prereqs
	@echo ; echo "===> ($(PKG_NAME)) make config ..."
	@test x != "x$(CONFIG_CMD)"
	@rm -f $(CONFIG_COOKIE) $(BUILD_COOKIE) $(INSTALL_COOKIE)
	$(CONFIG_CMD)
	touch $(CONFIG_COOKIE)

config:
	@if test -f $(NOBUILD_COOKIE) ; then  \
		echo "$(PKG_NAME): `cat $(NOBUILD_COOKIE)`" ;  \
	elif test ! -f $(CONFIG_COOKIE) ; then  \
		$(MAKE) reconfig ;  \
	fi

rebuild: config
	@echo ; echo "===> ($(PKG_NAME)) make build ..."
	@test x != "x$(BUILD_CMD)"
	@rm -f $(BUILD_COOKIE) $(INSTALL_COOKIE)
	$(BUILD_CMD)
	touch $(BUILD_COOKIE)

build:
	@if test -f $(NOBUILD_COOKIE) ; then  \
		echo "$(PKG_NAME): `cat $(NOBUILD_COOKIE)`" ;  \
	elif test ! -f $(BUILD_COOKIE) ; then  \
		$(MAKE) rebuild ;  \
	fi

reinstall: build
	@echo ; echo "===> ($(PKG_NAME)) make install ..."
	@test x != "x$(INSTALL_CMD)"
	@rm -f $(INSTALL_COOKIE)
	$(INSTALL_CMD)
	touch $(INSTALL_COOKIE)

install:
	@if test -f $(NOBUILD_COOKIE) ; then  \
		echo "$(PKG_NAME): `cat $(NOBUILD_COOKIE)`" ;  \
	elif test -f $(INSTALL_COOKIE) ; then  \
		echo "$(PKG_NAME): already installed" ;  \
	else  \
		$(MAKE) reinstall ;  \
	fi

clean:
	@echo ; echo "===> ($(PKG_NAME)) make clean ..."
	@test x != "x$(CLEAN_CMD)"
	$(CLEAN_CMD)

distclean:
	@echo ; echo "===> ($(PKG_NAME)) make distclean ..."
	@test x != "x$(DISTCLEAN_CMD)"
	@if test xdefault = "x$(DISTCLEAN_CMD)" ; then  \
		echo rm -rf "$(SRC_DIR)" "$(BUILD_DIR)" ;  \
		rm -rf "$(SRC_DIR)" "$(BUILD_DIR)" ;  \
		rm -f $(FETCH_COOKIE) $(CONFIG_COOKIE) ;  \
		rm -f $(BUILD_COOKIE) $(INSTALL_COOKIE) ;  \
	else  \
		echo "$(DISTCLEAN_CMD)" ;  \
		$(DISTCLEAN_CMD) ;  \
	fi

