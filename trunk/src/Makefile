# $Id$
# -*-makefile-*-
## * BeginRiceCopyright *****************************************************
## 
## Copyright ((c)) 2002, Rice University 
## All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
## 
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
## 
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
## 
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
## 
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage. 
## 
## ******************************************************* EndRiceCopyright *

#############################################################################
# Makefile
#############################################################################

# Environment options (exported variables):
#   - pass RELEASE=1 to compile a binary for release (no debugging info and 
#     with basic optimization)
#   - pass PURE=1 to create a purified library

#  Note: $(HPCTOOLKIT) should come from environment

#############################################################################
# Site/machine/architecture dependent configuration 
#############################################################################

include $(HPCTOOLKIT)/src/Makeinclude.config

#############################################################################
# Site/machine/architecture independent configuration 
#############################################################################

all: exes install

exes: build_hpcview build_bloop build_xprof #build_xcsprof

.PHONY: all exes post_cvs_co_script

#############################################################################

HSRC = $(HPCTOOLKIT_SRC_PATH)
HBIN = $(HPCTOOLKIT)/bin

#-----------------------------------------------------------------
#  Binaries
#-----------------------------------------------------------------

build_bloop: libs
	@$(MAKE) -f $(HSRC)/hpctoolkit/bloop/Makefile

build_hpcview: libs
	@$(MAKE) -f $(HSRC)/hpctoolkit/hpcview/Makefile

build_xprof: libs
	@$(MAKE) -f $(HSRC)/hpctoolkit/xprof/Makefile

build_xcsprof: libs
	@$(MAKE) -f $(HSRC)/hpctoolkit/xcsprof/Makefile

.PHONY: build_hpcview build_bloop build_xprof build_xcsprof


#-----------------------------------------------------------------
#  Libraries
#-----------------------------------------------------------------

libs: 
	@$(MAKE) -f $(HSRC)/lib/Makefile

.PHONY: libs


#-----------------------------------------------------------------
#  Install/Uninstall
#-----------------------------------------------------------------

INSTALL_EXES = $(EXE_HPCVIEW) $(EXE_BLOOP) $(EXE_XPROF) $(EXE_XCSPROF)

# install everything
install: exes uninstall install_scripts
	-mkdir -p $(HBIN)/$(PLATFORM)
	for i in $(INSTALL_EXES) ; do \
	  if [ -x $(HPCTOOLKIT_EXE_PATH)/$$i ] ; then \
	    cp $(HPCTOOLKIT_EXE_PATH)/$$i $(HBIN)/$(PLATFORM)/ ; \
	  fi \
	done

# uninstall everything
uninstall: uninstall_scripts
	-/bin/rm -f $(HBIN)/$(PLATFORM)/*


PERL_LIB     = perl

SCRIPTS_SRC  = ptran/ptran filters/f90modfilt hpcquick/hpcquick hpcquick/hpcquick-papiprof
SCRIPTS_INST = ptran f90modfilt hpcquick hpcquick-papiprof

# install perl tools
#   note: we only copy *.pm files (instead of the whole directory) so
#     that the CVS directory is not copied!
install_scripts: uninstall_scripts
	-mkdir $(HBIN)/$(PERL_LIB)
	cp $(HSRC)/lib/$(PERL_LIB)/*.pm $(HBIN)/$(PERL_LIB)/
	for i in $(SCRIPTS_SRC) ; do \
	  if [ -x $(HSRC)/hpctoolkit/$$i ] ; then \
	    cp $(HSRC)/hpctoolkit/$$i $(HBIN)/ ; \
	  fi \
	done;

# uninstall perl tools
uninstall_scripts:
	-/bin/rm -f -r $(HBIN)/$(PERL_LIB)
	for i in $(SCRIPTS_INST) ; do \
	  /bin/rm -f $(HBIN)/$$i ; \
	done

.PHONY: install uninstall install_scripts uninstall_scripts


#-----------------------------------------------------------------
#  Clean build files
#-----------------------------------------------------------------

CLEAN_DIRS = lib hpctoolkit/hpcview hpctoolkit/bloop hpctoolkit/xprof \
	hpctoolkit/xcsprof

clean:
	for i in $(CLEAN_DIRS) ; do \
	  if [ -d $(HSRC)/$$i ] ; then \
	    $(MAKE) clean -f $(HSRC)/$$i/Makefile ; \
	  fi \
	done

veryclean:
	if [ -n "$(HPCTOOLKIT_OBJ_PATH)" \
	     -a "$(HPCTOOLKIT_OBJ_PATH)" != "/" ] ; then \
	  /bin/rm -f -r $(HPCTOOLKIT_OBJ_PATH) ; \
	fi

.PHONY: clean veryclean


#-----------------------------------------------------------------
#  Vars
#-----------------------------------------------------------------

vars: 
	@echo CXX=$(CXX)
	@echo CC=$(CC)

.PHONY: vars
