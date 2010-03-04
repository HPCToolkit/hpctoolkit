# -*-Mode: makefile;-*-

## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## -----------------------------------
## Part of HPCToolkit (hpctoolkit.org)
## -----------------------------------
## 
## Copyright ((c)) 2002-2009, Rice University 
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
	papers.html \
	software.html \
	software-instructions.html \
	\
	info-acks.html \
	info-contact.html \
	info-people.html \
	\
	fig/index.html \
	fig/hpctoolkit-workflow.png \
	fig/hpcviewer-annotated-screenshot.jpg \
	fig/spacer.gif \
	\
	style/index.html \
	style/styles.css \
	style/footer-hpctoolkit.js \
	style/header-hpctoolkit.js \
	style/header.gif


#############################################################################
# Automake rules
#############################################################################

nobase_dist_doc_DATA = $(MYSOURCES)

#############################################################################
# Other rules
#############################################################################

