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

from ._objbimap import *

import pytest
import re

def test_adds_dels():
  a, b, c = (object() for _ in range(3))
  m = ObjectBimap()

  assert a not in m

  m[a] = b
  assert a in m and m[a] is b
  assert b in m and m[b] is a

  errAB = re.escape(f"Refusing to overwrite previous association for {a!r}: was {b!r}, request to be {c!r}")
  errBA = re.escape(f"Refusing to overwrite previous association for {b!r}: was {a!r}, request to be {c!r}")
  with pytest.raises(KeyError, match=errAB):
    m[a] = c
  with pytest.raises(KeyError, match=errAB):
    m[c] = a
  with pytest.raises(KeyError, match=errBA):
    m[c] = b

  m[b] = a
  assert a in m and m[a] is b
  assert b in m and m[b] is a

  assert m.get(a) is b
  assert m.get(c) is None

  m[c] = c
  assert c in m
  assert m[c] is c

  del m[a]
  assert a not in m and b not in m
  with pytest.raises(KeyError): m[a]
  with pytest.raises(KeyError): m[b]

  assert m.setdefault(c) is c

  assert m.pop(c) is c
  assert c not in m

  assert m.setdefault(c, a) is a
  assert m[c] is a

  m2 = ObjectBimap([(a, b), (c, c)])
  assert m2[a] is b and m2[c] is c

def test_len_iter():
  a, b, c, d, e, f = (object() for _ in range(6))
  m = ObjectBimap()

  assert len(m) == 0
  m[a] = b
  assert len(m) == 1
  m[c] = d
  assert len(m) == 2
  m[e] = e
  assert len(m) == 3
  m[f] = f
  assert len(m) == 4

  l = list(m)
  assert len(l) == 4
  assert l == [(a,b), (c,d), (e,e), (f,f)]

  m2 = m.copy()
  assert m2[a] is b and m2[c] is d and m2[e] is e and m2[f] is f

  m.clear()
  assert len(m) == 0 and len(list(m)) == 0
  m[b] = a
  assert len(m) == 1 and len(list(m)) == 1
  assert list(m) == [(b, a)]
