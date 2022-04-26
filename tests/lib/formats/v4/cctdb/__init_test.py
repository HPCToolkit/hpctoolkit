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
  a = ContextDB(open(testdatadir/'small_v4.0'/'cct.db'))

  zero = {'valueBlock': {}}

  m_1 = {2: {1: 152}, 17: {1: 1}}
  m_2i = {2: {1: 76}, 17: {1: 1}}
  m_2e = m_2i | {1: {1: 76}}
  m_3i = {17: {1: 1}}
  m_3e = {16: {1: 1}, 17: {1: 1}}
  m_4i = {2: {1: 76}}
  m_4e = m_4i | {1: {1: 75}}
  m_5i = {2: {1: 1}}
  m_5e = m_5i | {1: {1: 1}}

  b = ContextDB(
    ctxs = {
      'ctxs': [
        {'valueBlock': m_1},
        zero,
        {'valueBlock': m_1},
        {'valueBlock': m_1},
        {'valueBlock': m_2i},
        zero,
        {'valueBlock': m_2e},
        {'valueBlock': m_2e},
        zero, zero, zero, zero,
        ] + [{'valueBlock': m_3i}, zero]*13 + [
        {'valueBlock': m_3e},
        zero,
        {'valueBlock': m_4i},
        zero,
        {'valueBlock': m_4i},
        {'valueBlock': m_4i},
        zero,
        {'valueBlock': m_4e},
        {'valueBlock': m_4e},
        zero, zero,
        ] + [{'valueBlock': m_5i}, zero]*10 + [
        {'valueBlock': m_5e},
        zero, zero, zero, zero,
      ],
    }
  )

  assert str(a) == str(b)
  assert a.identical(b)

  assert 'object at 0x' not in repr(a)
  c = eval(repr(a))
  assert str(a) == str(c)
  assert a.identical(c)
