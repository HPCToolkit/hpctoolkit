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
## Copyright ((c)) 2022-2022, Rice University
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

from . import *

from pathlib import Path
import pytest

rootdir = Path(__file__).parent

def test_small_v4_0_meta():
  f = open(rootdir/'v4'/'testdata'/'small_v4.0'/'meta.db')
  a = general_unpack(f)
  assert type(a) is v4.MetaDB
  assert a.identical(v4.MetaDB(f))

@pytest.mark.xfail  # TODO: Need to rethink how general_unpack works
def test_small_v4_0_profile():
  f = open(rootdir/'v4'/'testdata'/'small_v4.0'/'profile.db')
  a = general_unpack(f)
  assert type(a) is v4.ProfileDB
  assert a.identical(v4.ProfileDB(f))

@pytest.mark.xfail  # TODO: Need to rethink how general_unpack works
def test_small_v4_0_cct():
  f = open(rootdir/'v4'/'testdata'/'small_v4.0'/'cct.db')
  a = general_unpack(f)
  assert type(a) is v4.ContextDB
  assert a.identical(v4.ContextDB(f))

@pytest.mark.xfail  # TODO: Need to rethink how general_unpack works
def test_small_v4_0_trace():
  f = open(rootdir/'v4'/'testdata'/'small_v4.0'/'trace.db')
  a = general_unpack(f)
  assert type(a) is v4.TraceDB
  assert a.identical(v4.TraceDB(f))
