# -*-Mode: makefile;-*-
# $Header$

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
#
# File:
#   $Source$
#
# Description:
#   *Process with automake to produce Makefile.in*
#
#   Note: All local variables are prefixed with MY to prevent name
#   clashes with automatic automake variables.
#
#############################################################################

# We do not want the standard GNU files (NEWS README AUTHORS ChangeLog...)
AUTOMAKE_OPTIONS = foreign

#############################################################################
# Local settings
#############################################################################

# A list of {file ! installation directory} pairs.
MYFILEPAIRS = \
	README! \
	README.Contact! \
	README.Index! \
	README.License! \
	README.ReleaseNotes! \
	Sourceme-csh! \
	Sourceme-sh! 

MYSCRIPTPAIRS = \
	src/config/hpcguess!config \
	src/config/hpcplatform!config \
	src/config/config.guess!config 

MYDIRS = \
	doc \
	examples \
	lib

MYINSTALLDIR = $(DESTDIR)$(prefix)

MYXERCESLIB = @XERCES@/lib
MYXERCESINSTALLDIR = $(MYINSTALLDIR)/xercesc

#############################################################################
# Automake rules
#############################################################################

SUBDIRS = \
	src

#############################################################################
# Other rules
#############################################################################

############################################################
# Install
############################################################

# *-hook runs after install has completed
install-data-local: myinstall-files myinstall-scripts \
	myinstall-dirs myinstall-xerces

myinstall-files:
	@for pair in $(MYFILEPAIRS); do \
	  src=`echo $${pair} | awk -F'!' '{print $$1}'` ; \
	  dir=`echo $${pair} | awk -F'!' '{print $$2}'` ; \
	  destdir=$(MYINSTALLDIR)/$${dir} ; \
	  $(mkinstalldirs) $${destdir} ; \
	  $(INSTALL_DATA) $(srcdir)/$${src} $${destdir} ; \
	done

myinstall-scripts:
	@for pair in $(MYSCRIPTPAIRS); do \
	  src=`echo $${pair} | awk -F'!' '{print $$1}'` ; \
	  dir=`echo $${pair} | awk -F'!' '{print $$2}'` ; \
	  destdir=$(MYINSTALLDIR)/$${dir} ; \
	  $(mkinstalldirs) $${destdir} ; \
	  $(INSTALL_SCRIPT) $(srcdir)/$${src} $${destdir} ; \
	done

myinstall-dirs:
	@for x in $(MYDIRS); do \
	  cp -R $(srcdir)/$$x $(MYINSTALLDIR)/. ; \
	done

myinstall-xerces: 
	@$(mkinstalldirs) $(MYXERCESINSTALLDIR)
	@cp -R $(MYXERCESLIB) $(MYXERCESINSTALLDIR)/.

.PHONY : install-data-hook myinstall-files myinstall-scripts \
	myinstall-dirs myinstall-xerces


############################################################
# WebDocs
#   The following extracts the various files that will appear on
#   the Web site as documentation for the current version.
#   The tarfile should be extracted in the "documentation" sub-directory
#   of the Web site.
#   Eventually, this could perhaps be done with 'make dist'...
############################################################

# GNUtar
MYTAR = tar
MYSRC = @abs_top_srcdir@

hpc_webdocs: 
	cd $(srcdir)/.. ; \
	${MYTAR} cvf $(MYINSTALLDIR)/@PACKAGE_NAME@-webdocs.tar \
	 --exclude=CVS \
	 HPCToolkit/README* HPCToolkit/Sourceme* \
	 HPCToolkit/doc \
	 HPCToolkit/lib/html \
	 HPCToolkit/lib/dtd/*.dtd

.PHONY: hpc_webdocs
