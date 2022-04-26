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

from itertools import count
from struct import Struct
import io
import mmap
import operator
import os
import resource
import struct

class _FileView:
  """
  Class that mmaps the relavent part of a file anytime a slice is accessed.
  """
  __slots__ = ['_fd', '_writable']

  def __init__(self, src):
    self._fd = os.dup(src.fileno())
    self._writable = bool(src.writable())

  def __del__(self):
    try:
      os.close(self._fd)
    except AttributeError: pass

  def __len__(self):
    return os.stat(self._fd).st_size

  def __getitem__(self, key):
    if not isinstance(key, slice):
      raise TypeError(f"This kind of view needs to be sliced before access")
    if key.step is not None and key.step != 1:
      raise ValueError(f"Mmapped file access does not support alternative step sizes")
    if key.stop is None:
      raise ValueError(f"Mmapped file slicing must have a concrete upper bound")
    start, stop = key.start if key.start is not None else 0, key.stop
    if stop <= start: return memoryview(b'')

    # Extend the file to have enough space if needed
    if os.stat(self._fd).st_size < key.stop:
      os.truncate(self._fd, key.stop)

    pagestart, pageoffset = divmod(start, mmap.ALLOCATIONGRANULARITY)
    pagestart *= mmap.ALLOCATIONGRANULARITY
    m = mmap.mmap(self._fd, offset=pagestart, length=pageoffset + stop - start,
                  access=mmap.ACCESS_WRITE if self._writable else mmap.ACCESS_READ)
    return memoryview(m)[pageoffset:pageoffset+stop-start]

def _view(src):
  if isinstance(src, _FileView): return src

  # Anything that can be turned into a memoryview should be
  try:
    return memoryview(src)
  except TypeError: pass

  return _FileView(src)

class VersionedStruct:
  """
  Like struct.Struct, but allows for the definition to shift as the version
  number increases. Packs and unpacks to a dict.
  """
  __slots__ = ['_fields']

  def __init__(self, _endian='>', /, **fields):
    self._fields = {}
    for name, (version, offset, form) in fields.items():
      self._fields[name] = (int(version), int(offset), Struct(_endian+str(form)))

  def size(self, version):
    return max(o + s.size for k,(v,o,s) in self._fields.items() if v <= version)

  def unpack_from(self, version, src, offset=0):
    src = _view(src)[offset:offset+self.size(version)]
    out = {}
    for k,(v,o,s) in self._fields.items():
      if v <= version:
        out[k], = s.unpack_from(src, o)
    return out

  def pack_into(self, version, dst, data, offset=0):
    dst = _view(dst)[offset:offset+self.size(version)]
    for k,(v,o,s) in self._fields.items():
      if v <= version:
        s.pack_into(dst, o, data[k])


class VersionedBitFlags:
  """
  VersionedStruct, but for flags in a bitfield. Interprets to and from a set()
  of all set flags.
  """
  __slots__ = ['_fields']

  def __init__(self, /, **fields):
    self._fields = {}
    for name, (version, bit) in fields.items():
      self._fields[name] = (int(version), 1<<int(bit))

  def normalize(self, version, src):
    """Convert an arbitrary iterable to a set of all set flags"""
    out = set()
    for x in src:
      if x in self._fields:
        (out.add if self._fields[x][0] <= version else out.discard)(x)
      else:
        try:
          k, v = x
        except TypeError:
          raise ValueError(f"Invalid entry for bitfield: {x}")
        else:
          if k not in self._fields:
            raise ValueError(f"Invalid entry for bitfield: {x}")
          (out.add if v and self._fields[k][0] <= version else out.discard)(k)
    return out

  def unpack(self, version, src):
    out = set()
    for k,(v,m) in self._fields.items():
      if v <= version and src & m != 0:
        out.add(k)
    return out

  def pack(self, version, values):
    values = self.normalize(version, values)
    out = 0
    for k,(v,b) in self._fields.items():
      if v <= version and k in values:
        out |= m
    return out


def unpack(s, src, offset=0):
  """
  struct.Struct.unpack, but handles the private details of view modulation.
  """
  return s.unpack_from(_view(src)[offset:offset+s.size], 0)

def array_unpack(s, n, src, offset=0):
  """
  Somewhat like struct.Struct.iter_unpack, but iterates exactly `n` times.
  """
  if n == 0: return tuple()
  src = _view(src)[offset:offset+s.size*n]
  for o in range(0, s.size*n, s.size):
    yield s.unpack_from(src, o)


def cached_property(cachevar):
  """
  Like functools.cached_property, but works with __slots__ given an alternative
  attribute to use as the cache.
  """
  def apply(method):
    def getter(self):
      try:
        return getattr(self, cachevar)
      except AttributeError: pass
      val = method(self)
      setattr(self, cachevar, val)
      return val
    def setter(self, val):
      setattr(self, cachevar, val)
    def deler(self):
      delattr(self, cachevar)
    return property(getter, setter, deler, doc=method.__doc__)
  return apply


def read_ntstring(src, offset=0):
  """
  Read a null-terminated UTF-8 string from a source.
  """
  src = _view(src)
  bits = b''
  for o in count(offset):
    b = bytes(src[o:o+1])
    if b == b'\0': break
    if len(b) == 0:
      raise ValueError(f"Null-terminated string was not terminated (started at offset {offset:d})")
    bits += b
  return bits.decode('utf-8')


def isomorphic_seq(a, b, /, eq, key=None):
  """
  Determine if two sequences (lists) are isomorphic or not, based on the given
  equality function `eq`. If `key` is given, the inputs are first sorted via
  this callable to reduce comparisons.
  """
  if len(a) != len(b): return False
  if key is not None:
    a, b = sorted(a, key=key), sorted(b, key=key)

  # O(N) pass: find matching objects in the same location. Ideally the key is
  # good enough that this happens often.
  ra, rb = [], []
  for av, bv in zip(a, b):
    if not eq(av, bv):
      ra.append(av)
      rb.append(bv)

  # O(N^2) pass: find a match for each mismatched object.
  for av in ra:
    for i,bv in enumerate(rb):
      if eq(av, bv):
        del rb[i]
        break
    else:
      return False
  return True
