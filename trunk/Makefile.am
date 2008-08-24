# -*-Mode: makefile;-*-
# $Id$

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

MYDATA = \
	README \
	README.Contact \
	README.Index \
	README.License
	README.ReleaseNotes

MYSCRIPTS = \
	Sourceme-csh \
	Sourceme-sh \
	config/hpcguess \
	config/hpcplatform \
	config/config.guess

MYXERCESCDIR = $(pkglibdir)/xercesc

MYMONITORDIR = $(pkglibdir)/monitor

#############################################################################
# Automake rules
#############################################################################

SUBDIRS = \
	src \
	\
	doc \
	lib

pkglibexecdir = $(libexecdir)/$(PACKAGE)
pkglibexec_SCRIPTS = $(MYSCRIPTS)

dist_doc_DATA = $(MYDATA)


#############################################################################
# Other rules
#############################################################################

############################################################
# Install
############################################################

# Note: *-local and *-hook are special automake versions of the
# regular target. *-hook runs *after* install has completed

install-data-local: myinstall-xerces myinstall-monitor

myinstall-xerces:
	if test 1; then \
	  $(mkinstalldirs) $(MYXERCESCDIR) ; \
	  cp -R @XERCES@/lib/* $(MYXERCESCDIR)/ ; \
	fi

myinstall-monitor:
	if test "@OPT_INSTALL_MONITOR@" = "yes"; then \
	  $(mkinstalldirs) $(MYMONITORDIR) ; \
	  cp -R @MONITOR@/lib*/* $(MYMONITORDIR)/ ; \
	fi


# A list of {file ! installation directory} pairs.
#MYSCRIPTPAIRS = \
#	config/hpcguess!config \
#	config/hpcplatform!config \
#	config/config.guess!config 
#
#myinstall-scripts:
#	for pair in $(MYSCRIPTPAIRS); do \
#	  src=`echo $${pair} | awk -F'!' '{print $$1}'` ; \
#	  dir=`echo $${pair} | awk -F'!' '{print $$2}'` ; \
#	  destdir=$(MYINSTALLDIR)/$${dir} ; \
#	  $(mkinstalldirs) $${destdir} ; \
#	  $(INSTALL_SCRIPT) $(srcdir)/$${src} $${destdir} ; \
#	done

.PHONY : myinstall-xerces myinstall-monitor


############################################################
# Dist
############################################################

dist-hook: my-dist

my-dist:
	rm -rf `find $(distdir) -name CVS -o -name ".svn"`

.PHONY: my-dist


############################################################
# Create hpctoolkit distribution
# 
# (Requires GNU tar)
#
# PACKAGE_NAME, PACKAGE_TARNAME, PACKAGE_VERSION
############################################################

MYTAR = tar
MYTOUCH = @abs_top_srcdir@/config/hpcautotouch
MYSRC = @abs_top_srcdir@
MYINFO = @abs_top_srcdir@/configure
MYPREFIX = @abs_top_builddir@

hpc-dist:
	ver=`grep "PACKAGE_VERSION=" $(MYINFO) | awk -F"'" '{print $$2}'` ; \
	top_dir=`$(SHELL) -c "cd $(MYSRC) && pwd"` ; \
	nm_cur=`basename $${top_dir}` ; \
	nm_new="hpctoolkit-$${ver}" ; \
	\
	cd $${top_dir}/.. ; \
	ln -s $${nm_cur} $${nm_new} ; \
	\
        $(MYTAR) zcvf $(MYPREFIX)/$${nm_new}.tar.gz \
	  --dereference --exclude="CVS" --exclude=".svn" \
	  \
	  $${nm_new}/aclocal.m4 \
	  $${nm_new}/autogen \
	  $${nm_new}/configure* \
	  $${nm_new}/Makefile* \
	  $${nm_new}/README* \
	  $${nm_new}/Sourceme-* \
	  \
	  $${nm_new}/config \
	  \
          $${nm_new}/doc \
	  $${nm_new}/examples \
          $${nm_new}/externals \
          $${nm_new}/lib \
          $${nm_new}/src \
	  $${nm_new}/tools \
	  ; \
	\
	rm $${nm_new} 

.PHONY: hpc-dist

############################################################
