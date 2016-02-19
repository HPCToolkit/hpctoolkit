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
AUTOMAKE_OPTIONS = foreign

#############################################################################
# Local settings
#############################################################################

MYSOURCES = \
	hpctoolkit.tex \
	\
	hpcrun.tex \
	hpclink.tex \
	hpcrun-flat.tex \
	\
	hpcstruct.tex \
	\
	hpcprof.tex \
	hpcprof-mpi.tex \
	hpcprof-flat.tex \
	hpcproftt.tex \
	hpcsummary.tex \
	\
	hpctraceviewer.tex \
	hpcviewer.tex

MYLATEX2MAN = @abs_top_srcdir@/tools/latex2man/latex2man
MYLATEX2MANTRANS = @srcdir@/hpctoolkit-latex2man.trans

MYMANEXT = 1hpctoolkit

MYMAN     = $(patsubst %.tex,%.$(MYMANEXT),$(MYSOURCES))
MYMANHTML = $(patsubst %.tex,%.html,$(MYSOURCES))

#############################################################################
# Automake rules
#############################################################################

# BUG! Won't work with $(MYMAN) variable!
#man_MANS = $(MYMAN)
man_MANS = \
	hpctoolkit.1hpctoolkit \
	hpcrun.1hpctoolkit \
	hpcrun-flat.1hpctoolkit \
	hpclink.1hpctoolkit \
	hpcstruct.1hpctoolkit \
	hpcprof.1hpctoolkit \
	hpcprof-flat.1hpctoolkit \
	hpcproftt.1hpctoolkit \
	hpcsummary.1hpctoolkit \
	hpctraceviewer.1hpctoolkit \
	hpcviewer.1hpctoolkit

wwwmandir = $(docdir)/man
dist_wwwman_DATA = $(MYMANHTML)

CLEANFILES = $(MYMAN) $(MYMANHTML)

#############################################################################
# Other rules
#############################################################################

%.tex.pp : %.tex
	sed -e 's/=PACKAGE_VERSION=/@PACKAGE_VERSION@/g' $< > $@
#	$(MYCPP) -P -I@builddir@ $< $@

%.$(MYMANEXT) : %.tex.pp
	$(MYLATEX2MAN) -t $(MYLATEX2MANTRANS) $< $@

%.html : %.tex.pp
	$(MYLATEX2MAN) -H -t $(MYLATEX2MANTRANS) $< $@
