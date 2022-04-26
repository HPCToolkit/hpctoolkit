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
from ..metadb import MetaDB
from ..profiledb import ProfileDB

from pathlib import Path

testdatadir = Path(__file__).parent.parent / 'testdata'

def test_small_v4_0():
  metadb = MetaDB(open(testdatadir/'small_v4.0'/'meta.db'))
  C = metadb.context.byCtxId()
  profiledb = ProfileDB(open(testdatadir/'small_v4.0'/'profile.db'), metadb=metadb)
  P = profiledb.profiles.profiles

  a = TraceDB(open(testdatadir/'small_v4.0'/'trace.db'), metadb=metadb, profiledb=profiledb)

  b = TraceDB(metadb=metadb, profiledb=profiledb,
    ctxTraces = {
      'minTimestamp': 1650412082819499000, 'maxTimestamp': 1650412082820356000,
      'traces': [
        { 'profile': P[1], 'trace': [
          (1650412082819499000, C[1]),
          (1650412082820356000, C[38]),
        ]},
      ],
    }
  )

  assert str(a) == str(b)
  assert a.identical(b)
