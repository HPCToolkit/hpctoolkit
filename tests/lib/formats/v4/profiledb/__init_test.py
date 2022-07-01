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

from pathlib import Path

testdatadir = Path(__file__).parent.parent / 'testdata'

def test_small_v4_0():
  metadb = MetaDB(open(testdatadir/'small_v4.0'/'meta.db'))
  C = metadb.context.byCtxId()
  S,M = metadb.metrics.byStatMetricId(), metadb.metrics.byPropMetricId()

  a = ProfileDB(open(testdatadir/'small_v4.0'/'profile.db'), metadb=metadb)

  s_1i = {S[15]: 1, S[16]: 1, S[17]: 1, S[20]: 3e9, S[23]: 3e9, S[26]: 3e9, S[29]: 3e9}
  s_2e = {S[15]: 1, S[16]: 1, S[17]: 1, S[19]: 1e9, S[20]: 1e9, S[22]: 1e9, S[23]: 1e9,
          S[25]: 1e9, S[26]: 1e9, S[28]: 1e9, S[29]: 1e9}
  s_3i = {S[15]: 1, S[16]: 1, S[17]: 1, S[20]: 2e9, S[23]: 2e9, S[26]: 2e9, S[29]: 2e9}
  s_3e = s_3i | {S[19]: 2e9, S[22]: 2e9, S[25]: 2e9, S[28]: 2e9}

  m_1i = {M[17]: 3e9}
  m_2e = {M[16]: 1e9, M[17]: 1e9}
  m_3i = {M[17]: 2e9}
  m_3e = m_3i | {M[16]: 2e9}

  b = ProfileDB(metadb=metadb,
    profiles = {
      'profiles': [
        {
          'idTuple': 'Summary',
          'valueBlock': {
            C[0]: s_1i, C[1]: s_1i, C[2]: s_1i, C[3]: s_1i,
            C[5]: s_2e, C[6]: s_2e, C[7]: s_2e,
            C[12]: s_3i, C[13]: s_3i, C[15]: s_3e, C[16]: s_3e, C[17]: s_3e,
          },
          'flags': {'isSummary'},
        },
        {
          'idTuple': (
            {'kind': 1, 'logicalId': 0, 'physicalId': 0x7f0101, 'flags': ['isPhysical']},
            {'kind': 3, 'logicalId': 0, 'physicalId': 0},
          ),
          'valueBlock': {
            C[0]: m_1i, C[1]: m_1i, C[2]: m_1i, C[3]: m_1i,
            C[5]: m_2e, C[6]: m_2e, C[7]: m_2e,
            C[12]: m_3i, C[13]: m_3i, C[15]: m_3e, C[16]: m_3e, C[17]: m_3e,
          },
        },
      ],
    }
  )

  assert str(a) == str(b)
  assert a.identical(b)
