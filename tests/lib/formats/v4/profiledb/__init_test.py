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

testdatadir = Path(__file__).parent.parent / 'testdata'

def test_small_v4_0():
  a = ProfileDB(open(testdatadir/'small_v4.0'/'profile.db'))

  m_1 = { 0: 1, 1: 1, 2: 1, 5: 152, 8: 152, 11: 152, 14: 152, 15: 1, 16: 1,
          17: 1, 20: 1, 23: 1, 26: 1, 29: 1 }
  m_2i = { 0: 1, 1: 1, 2: 1, 5: 76, 8: 76, 11: 76, 14: 76, 15: 1, 16: 1, 17: 1,
          20: 1, 23: 1, 26: 1, 29: 1 }
  m_2e = m_2i | { 4: 76, 7: 76, 10: 76, 13: 76 }
  m_3i = { 15: 1, 16: 1, 17: 1, 20: 1, 23: 1, 26: 1, 29: 1 }
  m_3e = m_3i | { 19: 1, 22: 1, 25: 1, 28: 1 }
  m_4i = { 0: 1, 1: 1, 2: 1, 5: 76, 8: 76, 11: 76, 14: 76 }
  m_4e = m_4i | { 4: 75, 7: 75, 10: 75, 13: 75 }
  m_5i = { 0: 1, 1: 1, 2: 1, 5: 1, 8: 1, 11: 1, 14: 1 }
  m_5e = m_5i | { 4: 1, 7: 1, 10: 1, 13: 1 }

  m_6 = { 2: 152, 17: 1 }
  m_7i = { 2: 76, 17: 1 }
  m_7e = m_7i | { 1: 76 }
  m_8i = { 17: 1 }
  m_8e = m_8i | { 16: 1 }
  m_9i = { 2: 76 }
  m_9e = m_9i | { 1: 75 }
  m_ai = { 2: 1 }
  m_ae = m_ai | { 1: 1 }

  b = ProfileDB(
    profiles = {
      'profiles': [
        {
          'idTuple': 'Summary',
          'valueBlock': {
            0: m_1, 2: m_1, 3: m_1,
            4: m_2i, 6: m_2e, 7: m_2e,
            12: m_3i, 14: m_3i, 16: m_3i, 18: m_3i, 20: m_3i, 22: m_3i,
            24: m_3i, 26: m_3i, 28: m_3i, 30: m_3i, 32: m_3i, 34: m_3i,
            36: m_3i, 38: m_3e,
            40: m_4i, 42: m_4i, 43: m_4i, 45: m_4e, 46: m_4e,
            49: m_5i, 51: m_5i, 53: m_5i, 55: m_5i, 57: m_5i, 59: m_5i,
            61: m_5i, 63: m_5i, 65: m_5i, 67: m_5i, 69: m_5e,
          },
        },
        {
          'idTuple': (
            {'kind': 1, 'logicalId': 0, 'physicalId': 0x7f0101, 'flags': ['isPhysical']},
            {'kind': 3, 'logicalId': 0, 'physicalId': 0},
          ),
          'valueBlock': {
            0: m_6, 2: m_6, 3: m_6,
            4: m_7i, 6: m_7e, 7: m_7e,
            12: m_8i, 14: m_8i, 16: m_8i, 18: m_8i, 20: m_8i, 22: m_8i,
            24: m_8i, 26: m_8i, 28: m_8i, 30: m_8i, 32: m_8i, 34: m_8i,
            36: m_8i, 38: m_8e,
            40: m_9i, 42: m_9i, 43: m_9i, 45: m_9e, 46: m_9e,
            49: m_ai, 51: m_ai, 53: m_ai, 55: m_ai, 57: m_ai, 59: m_ai,
            61: m_ai, 63: m_ai, 65: m_ai, 67: m_ai, 69: m_ae,
          },
        },
      ],
    }
  )

  assert str(a) == str(b)
  assert a == b

  assert 'object at 0x' not in repr(a)
  c = eval(repr(a))
  assert str(a) == str(c)
  assert a == c
