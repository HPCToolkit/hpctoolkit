# -*-Mode: makefile;-*-

## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2002-2016, Rice University
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
##
## File:
##   $HeadURL$
##
## Description:
##   *Process with automake to produce Makefile.in*
##
##   Note: All local variables are prefixed with MY to prevent name
##   clashes with automatic automake variables.
##
#############################################################################

# We do not want the standard GNU files (NEWS README AUTHORS ChangeLog...)
AUTOMAKE_OPTIONS = foreign no-dist

#############################################################################
# Local settings
#############################################################################

MYDATA = \
	README \
	README.License \
	README.ReleaseNotes \
	README.Acknowledgments

MYSCRIPTS = \
	config/hpcguess \
	config/hpcplatform \
	config/config.guess


#############################################################################
# Automake rules
#############################################################################

pkglibdir = @my_pkglibdir@
pkglibexecdir = @my_pkglibexecdir@

SUBDIRS = src

if OPT_BUILD_FRONT_END
SUBDIRS +=  \
	doc \
	lib

pkglibexec_SCRIPTS = $(MYSCRIPTS)
dist_doc_DATA = $(MYDATA)
endif


#############################################################################
# Other rules
#############################################################################

############################################################
# Install
############################################################

# Note: *-local and *-hook are special automake versions of the
# regular target. *-hook runs *after* install has completed

#
# Copy the libraries in pattern ($3) from absolute directory ($2) to a
# local directory below prefix, and do the copy if ($1) is 'yes'.
# It's up to configure to decide if the library exists, whether to
# copy it and where to put it, so we don't treat errors as fatal here.
#
BASE_LIBS_DIR = $(DESTDIR)$(pkglibdir)
EXT_LIBS_DIR = $(DESTDIR)$(pkglibdir)/ext-libs
MYMONITORDIR = $(DESTDIR)$(pkglibdir)/monitor

define copy-libs
    @if test "$1" = yes ; then \
	( cd "$2" && tar cf - $3 ) | ( cd "$(EXT_LIBS_DIR)" && tar xvf - ) ; \
	true ; \
    fi
endef

install-exec-local: myinstall-monitor
	@echo "Copying external package libraries ..."
	$(mkinstalldirs) "$(EXT_LIBS_DIR)"
	$(call copy-libs,$(LIBDWARF_COPY),$(LIBDWARF_LIB),libdwarf*)
	$(call copy-libs,$(LIBELF_COPY),$(LIBELF_LIB),libelf*)
	$(call copy-libs,$(LIBMONITOR_COPY),$(LIBMONITOR_LIB),libmonitor*)
	$(call copy-libs,$(LIBUNWIND_COPY),$(LIBUNWIND_LIB),libunwind*)
	$(call copy-libs,$(SYMTABAPI_COPY),$(SYMTABAPI_LIB),libsymtabAPI*)
	$(call copy-libs,$(SYMTABAPI_COPY),$(SYMTABAPI_LIB),libcommon*)
	$(call copy-libs,$(SYMTABAPI_COPY_DYN),$(SYMTABAPI_LIB),libdyn*)
	$(call copy-libs,$(SYMTABAPI_COPY_PARSE),$(SYMTABAPI_LIB),libparse*)
	$(call copy-libs,$(SYMTABAPI_COPY_PARSE),$(SYMTABAPI_LIB),libinstruct*)
	$(call copy-libs,$(XED2_COPY),$(XED2_LIB_DIR),lib*)
	$(call copy-libs,$(XERCES_COPY),$(XERCES_LIB),libxerces*)

myinstall-monitor:
	if test "@OPT_INSTALL_MONITOR@" = "yes"; then \
	  $(mkinstalldirs) $(MYMONITORDIR) ; \
	  cp -R @MONITOR@/lib*/libmonitor* $(MYMONITORDIR)/ ; \
	fi

.PHONY: myinstall-monitor

# Strip the debug sections from the libraries (ours and externals)
# used in hpcrun and hpclink.  This prevents our code from being
# displayed in the viewer.
#
#  $1 = directory containing the libraries
#  $2 = glob pattern for library name
#

define strip-debug
    @if cd "$1" 2>/dev/null ; then  \
	for lib in $2 ; do  \
	    if test -f "$$lib" && test ! -L "$$lib" ; then  \
		echo "$$lib" ;  \
		objcopy --strip-debug "$$lib" ;  \
	    fi ;  \
	    true ; \
	done ;  \
    fi
endef

if OPT_STRIP_HPCRUN_LIBS
install-exec-hook:
	@echo "Strip debug sections from hpcrun libraries ..."
	$(call strip-debug,$(BASE_LIBS_DIR),libhpcrun*.so*)
	$(call strip-debug,$(BASE_LIBS_DIR),libhpcrun.o)
	$(call strip-debug,$(BASE_LIBS_DIR),libhpcrun*wrap*.a)
	$(call strip-debug,$(BASE_LIBS_DIR),libhpctoolkit.so*)
	$(call strip-debug,$(BASE_LIBS_DIR),libhpctoolkit.a)
	$(call strip-debug,$(EXT_LIBS_DIR),libmonitor.so*)
	$(call strip-debug,$(EXT_LIBS_DIR),libmonitor_wrap.a)
	$(call strip-debug,$(EXT_LIBS_DIR),libunwind*.so*)
	$(call strip-debug,$(EXT_LIBS_DIR),libunwind*.a)
	$(call strip-debug,$(MYMONITORDIR),libmonitor.so*)
endif


############################################################
# Dist
############################################################

DIST_FILES = Makefile* README* aclocal.m4 autogen configure*  \
	config doc lib src tools

MY_DIST_DIR = $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)
MY_TAR_FILE = $(MY_DIST_DIR).tar
SRCDIR = @srcdir@

dist hpc-dist:
	@echo "cleaning source tree (ignore errors) ..."
	-cd "$(SRCDIR)" && test -f Makefile && $(MAKE) clean
	rm -f "$(MY_DIST_DIR)" "$(MY_TAR_FILE)" "$(MY_TAR_FILE).gz"
	ln -s "$(SRCDIR)" "$(MY_DIST_DIR)"
	tar cf "$(MY_TAR_FILE)"  \
		--exclude .git   \
		--exclude .svn   \
		$(addprefix $(MY_DIST_DIR)/,$(DIST_FILES))
	rm -f "$(MY_DIST_DIR)"
	gzip "$(MY_TAR_FILE)"
	@ls -l "$(MY_TAR_FILE).gz"

.PHONY: dist hpc-dist

############################################################
