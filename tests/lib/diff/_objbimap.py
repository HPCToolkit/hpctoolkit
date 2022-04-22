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

import itertools

class ObjectBimap(dict):
  """
  One-to-one bi-directional mapping between objects. Does not require any
  properties on the objects themselves (ie. key is identity).
  """
  __slots__ = ['_left']

  def __init__(self, initial=None):
    super().__init__()
    self._left = set()
    if initial is not None:
      for k,k2 in (initial.items() if isinstance(initial, dict) else initial):
        self[k] = k2
        assert self[k2] is k

  def __getitem__(self, key):
    try:
      return super().__getitem__(id(key))
    except KeyError:
      raise KeyError(key) from None

  def get(self, key, *args, **kwargs):
    return super().get(id(key), *args, **kwargs)

  def pop(self, key, *args, **kwargs):
    try:
      return super().pop(id(key), *args, **kwargs)
    except KeyError:
      raise KeyError(key) from None

  def __contains__(self, key):
    return super().__contains__(id(key))

  def __setitem__(self, key, key2):
    # Sanity check: if one of the keys is in the map already, but doesn't point
    # to key2, we refuse to break the other pair to make this one.
    if key in self:
      if self[key] is not key2:
        raise KeyError(f"Refusing to overwrite previous association for {key!r}: was {self[key]!r}, request to be {key2!r}")
    if key2 in self:
      if self[key2] is not key:
        raise KeyError(f"Refusing to overwrite previous association for {key2!r}: was {self[key2]!r}, request to be {key!r}")

    # Add the pair of pointers
    super().__setitem__(id(key), key2)
    if key is not key2:
      super().__setitem__(id(key2), key)

    # Mark key as a left-side element, if key2 is not already.
    if id(key2) not in self._left:
      self._left.add(id(key))

  def setdefault(self, key, *args, **kwargs):
    return super().setdefault(id(key), *args, **kwargs)

  def __delitem__(self, key):
    key2 = self[key]
    super().__delitem__(id(key))
    if key is not key2:
      super().__delitem__(id(key2))

    assert id(key) in self._left or id(key2) in self._left
    self._left.discard(id(key))
    self._left.discard(id(key2))

  def __len__(self):
    return len(self._left)

  def __iter__(self):
    def pair(ki):
      k2 = super(ObjectBimap, self).__getitem__(ki)
      return (self[k2], k2)
    return map(pair, filter(lambda ki: ki in self._left, super().__iter__()))

  def __repr__(self):
    return (self.__class__.__name__ + "(["
            + ', '.join(f"({k!r}, {k2!r})" for k,k2 in self) + "])")

  def clear(self):
    super().clear()
    self._left.clear()

  def copy(self):
    x = self.__class__()
    for ki in super().__iter__():
      k2 = super().__getitem__(ki)
      x[k2] = self[k2]
    return x

  def __or__(self, other):
    if not isinstance(other, ObjectBimap): raise TypeError(type(other))
    return ObjectBimap(itertools.chain(iter(self), iter(other)))
  __ror__ = None
  def __ior__(self, other):
    if not isinstance(other, ObjectBimap): raise TypeError(type(other))
    for k, k2 in other:
      self[k] = k2
    return self
  def update(self, other):
    self |= other

  keys, values, items = None, None, None
  fromkeys, popitem = None, None
