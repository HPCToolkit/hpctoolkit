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

  m_1 = { S[0]: 1, S[1]: 1, S[2]: 1, S[5]: 152, S[8]: 152, S[11]: 152,
          S[14]: 152, S[15]: 1, S[16]: 1, S[17]: 1, S[20]: 1, S[23]: 1,
          S[26]: 1, S[29]: 1 }
  m_2i = { S[0]: 1, S[1]: 1, S[2]: 1, S[5]: 76, S[8]: 76, S[11]: 76, S[14]: 76,
           S[15]: 1, S[16]: 1, S[17]: 1, S[20]: 1, S[23]: 1, S[26]: 1, S[29]: 1 }
  m_2e = m_2i | { S[4]: 76, S[7]: 76, S[10]: 76, S[13]: 76 }
  m_3i = { S[15]: 1, S[16]: 1, S[17]: 1, S[20]: 1, S[23]: 1, S[26]: 1, S[29]: 1 }
  m_3e = m_3i | { S[19]: 1, S[22]: 1, S[25]: 1, S[28]: 1 }
  m_4i = { S[0]: 1, S[1]: 1, S[2]: 1, S[5]: 76, S[8]: 76, S[11]: 76, S[14]: 76 }
  m_4e = m_4i | { S[4]: 75, S[7]: 75, S[10]: 75, S[13]: 75 }
  m_5i = { S[0]: 1, S[1]: 1, S[2]: 1, S[5]: 1, S[8]: 1, S[11]: 1, S[14]: 1 }
  m_5e = m_5i | { S[4]: 1, S[7]: 1, S[10]: 1, S[13]: 1 }

  m_6 = { M[2]: 152, M[17]: 1 }
  m_7i = { M[2]: 76, M[17]: 1 }
  m_7e = m_7i | { M[1]: 76 }
  m_8i = { M[17]: 1 }
  m_8e = m_8i | { M[16]: 1 }
  m_9i = { M[2]: 76 }
  m_9e = m_9i | { M[1]: 75 }
  m_ai = { M[2]: 1 }
  m_ae = m_ai | { M[1]: 1 }

  b = ProfileDB(metadb=metadb,
    profiles = {
      'profiles': [
        {
          'idTuple': 'Summary',
          'valueBlock': {
            C[0]: m_1, C[2]: m_1, C[3]: m_1,
            C[4]: m_2i, C[6]: m_2e, C[7]: m_2e,
            C[12]: m_3i, C[14]: m_3i, C[16]: m_3i, C[18]: m_3i, C[20]: m_3i,
            C[22]: m_3i, C[24]: m_3i, C[26]: m_3i, C[28]: m_3i, C[30]: m_3i,
            C[32]: m_3i, C[34]: m_3i, C[36]: m_3i, C[38]: m_3e,
            C[40]: m_4i, C[42]: m_4i, C[43]: m_4i, C[45]: m_4e, C[46]: m_4e,
            C[49]: m_5i, C[51]: m_5i, C[53]: m_5i, C[55]: m_5i, C[57]: m_5i,
            C[59]: m_5i, C[61]: m_5i, C[63]: m_5i, C[65]: m_5i, C[67]: m_5i,
            C[69]: m_5e,
          },
        },
        {
          'idTuple': (
            {'kind': 1, 'logicalId': 0, 'physicalId': 0x7f0101, 'flags': ['isPhysical']},
            {'kind': 3, 'logicalId': 0, 'physicalId': 0},
          ),
          'valueBlock': {
            C[0]: m_6, C[2]: m_6, C[3]: m_6,
            C[4]: m_7i, C[6]: m_7e, C[7]: m_7e,
            C[12]: m_8i, C[14]: m_8i, C[16]: m_8i, C[18]: m_8i, C[20]: m_8i,
            C[22]: m_8i, C[24]: m_8i, C[26]: m_8i, C[28]: m_8i, C[30]: m_8i,
            C[32]: m_8i, C[34]: m_8i, C[36]: m_8i, C[38]: m_8e,
            C[40]: m_9i, C[42]: m_9i, C[43]: m_9i, C[45]: m_9e, C[46]: m_9e,
            C[49]: m_ai, C[51]: m_ai, C[53]: m_ai, C[55]: m_ai, C[57]: m_ai,
            C[59]: m_ai, C[61]: m_ai, C[63]: m_ai, C[65]: m_ai, C[67]: m_ai,
            C[69]: m_ae,
          },
        },
      ],
    }
  )

  assert str(a) == str(b)
  assert a.identical(b)
