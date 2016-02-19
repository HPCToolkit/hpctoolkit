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
	index.html \
	\
	documentation.html \
	examples.html \
	overview.html \
	publications.html \
	software.html \
	software-instructions.html \
	\
	info-acks.html \
	info-people.html

MYSOURCES_IN = \
	$(abs_srcdir)/publications.html.in

MYSOURCES_TGT = \
	$(addprefix $(srcdir)/, $(MYSOURCES))

MYDATA = \
	$(MYSOURCES) \
	\
	hpctoolkit.org.sitemap.txt \
	\
	googleeeb6a75d4102e1ef.html \
	\
	fig/index.html \
	fig/hpctoolkit-workflow.png \
	fig/hpcviewer-annotated-screenshot.jpg \
	fig/spacer.gif \
	\
	style/index.html \
	style/style.css \
	style/footer-hpctoolkit.js \
	style/header-hpctoolkit.js \
	style/header.gif


#############################################################################
# Automake rules
#############################################################################

# N.B.: Do not include $(MYSOURCES_IN) b/c it depends on bib2xhtml
BUILT_SOURCES = $(MYSOURCES_TGT)

nobase_dist_doc_DATA = $(MYDATA)

#############################################################################
# Other rules
#############################################################################

# For MacOS add/remove: | tr '\n' ' '

MY_AWKVARS= \
  -v varhdr="`cat $(srcdir)/style/header-hpctoolkit.html | tr '\n' ' '`" \
  -v varftr="`cat $(srcdir)/style/footer-hpctoolkit.html | tr '\n' ' '`"

MY_AWKPROG = '\
  BEGIN { } \
  /@header-hpctoolkit@/ { printf "%s", varhdr ; next; } \
  /@footer-hpctoolkit@/ { printf "%s", varftr ; next; } \
  { print; } \
  END { } \
  '

MY_VARSRCS = \
	$(srcdir)/style/header-hpctoolkit.html \
	$(srcdir)/style/footer-hpctoolkit.html

MY_PUBS = $(abs_top_srcdir)/doc/manual/texmf/bibtex/bib/pubs.bib

# Note: bib2xhtml supports 'pdf' and 'url' fields.
BIB2XHTML_ROOT = /opt/tallent/bib2xhtml
BIB2XHTML = $(BIB2XHTML_ROOT)/bib2xhtml

hpc: $(MYSOURCES_IN) $(MYSOURCES_TGT)

$(abs_srcdir)/publications.html.in : # $(MY_PUBS) # avoid auto regeneration
	@echo "*** BIB2XHTML_ROOT=$(BIB2XHTML_ROOT) ***"
	@if test -x $(BIB2XHTML) ; then \
	  cd $(BIB2XHTML_ROOT) && \
	    $(BIB2XHTML) -s unsort -i -d pubs-overview     $(MY_PUBS) $@ && \
	    $(BIB2XHTML) -s unsort -i -d pubs-hpctoolkit-a $(MY_PUBS) $@ && \
	    $(BIB2XHTML) -s unsort -i -d pubs-hpctoolkit-b $(MY_PUBS) $@ && \
	    $(BIB2XHTML) -s unsort -i -d pubs-prediction   $(MY_PUBS) $@ ; \
	else \
	  echo "*** Cannot run $(BIB2XHTML) ***" ; \
	fi

$(srcdir)/%.html : $(srcdir)/%.html.in $(MY_VARSRCS)
	$(AWK) $(MY_AWKVARS) $(MY_AWKPROG) $< > $@

.PHONY : hpc $(abs_srcdir)/publications.html.in

#$(MY_HTMLTGT): %.html : style/header.html %.html.in style/footer.html
#	cat $^ > $@
