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

from .._objbimap import ObjectBimap

class MatchResult:
  """Result of a matching operation"""
  __slots__ = ['full', 'matches']
  def __init__(self, full, matches=None):
    if matches is None: matches = ObjectBimap()
    self.full: bool = bool(full)
    assert isinstance(matches, ObjectBimap)
    self.matches: ObjectBimap = matches

  @classmethod
  def matchif(cls, a, b, cond=True):
    return MatchResult(True, ObjectBimap(((a, b),))) if cond else MatchResult(False)

  def __bool__(self):
    return self.full

  def __ior__(self, other):
    if not isinstance(other, MatchResult): raise TypeError(type(other))
    self.full = self.full and other.full
    self.matches |= other.matches
    return self

  def __or__(self, other):
    if not isinstance(other, MatchResult): raise TypeError(type(other))
    return MatchResult(self.full and other.full, self.matches | other.matches)

  def __repr__(self):
    return f"{self.__class__.__name__}({self.full}, {self.matches!r})"

def check_tyb(b, ty):
  if not isinstance(b, ty):
    raise TypeError(f"Cannot compare against {b!r}: expected an object of type {ty!r}")

def cmp_id(a, b, /, mr):
  assert isinstance(mr, MatchResult)
  if a is b: return True
  if a not in mr.matches: return False
  if mr.matches[a] is b: return True
  return False

def merge_unordered(a, b, /, match, key=None) -> MatchResult:
  """
  Find matches between two sequences, which are considered unordered.
  If `key` is given, it is used as a hint to assist with finding matches.
  Returns an ObjectBimap with the matching objects.
  """
  if key is not None:
    a, b = sorted(a, key=key), sorted(b, key=key)
  else:
    b = list(b)
  out = MatchResult(True)

  # TODO: Figure out a really clever algorithm to make this work better.

  # O(N^2) fallback pass:
  for av in a:
    for i,bv in enumerate(b):
      m = match(av, bv)
      if m:
        out |= m
        del b[i]
        break
    else:
      out.full = False
  out.full = len(b) == 0

  return out
