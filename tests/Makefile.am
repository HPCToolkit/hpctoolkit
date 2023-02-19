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
## Copyright ((c)) 2002-2023, Rice University
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

# This Makefile integrates the smoke tests with automake's 'make
# check'.  First, run configure, make, make install, then cd to this
# directory and run:
#
#   make check
#
# Note:
# 1. run-sort refers to the installed files (hpcrun, etc), so this
# must come after 'make install'.
#
# 2. Automake refers to paths in the build tree, so you must have the
# build tree expanded and run this from the build tree.  (This is why
# it doesn't work with 'spack test run', those tests are run later,
# after install.)
#
# 3. But it does work at spack install time, since the build directory
# still exists.
#
#   spack install --test=root <pkg>
#
# ***************************************************************************

# We do not want the standard GNU files (NEWS README AUTHORS ChangeLog...)
AUTOMAKE_OPTIONS = foreign

TESTS = run-sort

check_PROGRAMS = sort

sort_SOURCES = sort.cpp
sort_CXXFLAGS = -g -O @cxx17_flag@

clean-local:
	rm -rf hpctoolkit-*-measurements hpctoolkit-*-database
	rm -f *.hpcstruct
