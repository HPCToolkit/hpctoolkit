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

from ..._base import VersionedFormat
from ..._util import unpack, cached_property, VersionedBitFlags, isomorphic_seq

from ._load_modules import LoadModulesSection, LoadModule
from ._source_files import SourceFilesSection, SourceFile
from ._functions import FunctionsSection, Function

import struct
import enum
import os.path
import textwrap

def _parse_contexts(version, src, *, size, offset):
  out = []
  maxoffset = offset + size
  while offset < maxoffset:
    out.append(Context(version, src, offset=offset))
    offset += out[-1]._total_size
  return out


@VersionedFormat.minimize
class ContextTreeSection(VersionedFormat,
    # Added in v4.0
    _szRoots = (0, 0x00, 'Q'),
    _pRoots = (0, 0x08, 'Q'),
  ):
  """meta.db Context Tree section."""
  __slots__ = ['_lmTable', '_sfTable', '_fTable', '_roots']

  def _init_(self, *, roots=[]):
    super()._init_()
    self.roots = [c if isinstance(c, Context) else Context(**c)
                  for c in roots]

  def unpack_from(self, version, src, /, *args, lmSec, sfSec, fSec, **kwargs):
    if not isinstance(lmSec, LoadModulesSection):
      raise TypeError(f"lmSec must be a LoadModulesSection, got {type(lmSec)}")
    if not isinstance(sfSec, SourceFilesSection):
      raise TypeError(f"sfSec must be a SourceFilesSection, got {type(sfSec)}")
    if not isinstance(fSec, FunctionsSection):
      raise TypeError(f"fSec must be a FunctionsSection, got {type(fSec)}")
    self._lmTable = {m._srcoffset: m for m in lmSec.modules if hasattr(m, '_srcoffset')}
    self._sfTable = {f._srcoffset: f for f in sfSec.files if hasattr(f, '_srcoffset')}
    self._fTable = {f._srcoffset: f for f in fSec.functions if hasattr(f, '_srcoffset')}
    super().unpack_from(version, src, *args, **kwargs)

  @cached_property('_roots')
  @VersionedFormat.subunpack(list)
  def roots(self, *args):
    r = _parse_contexts(*args, size=self._szRoots, offset=self._pRoots)
    for c in r:
      c._lmTable, c._sfTable, c._fTable = self._lmTable, self._sfTable, self._fTable
    return r

  def __eq__(self, other):
    if not isinstance(other, ContextTreeSection): return NotImplemented
    return isomorphic_seq(self.roots, other.roots,
                          key=lambda c: (c.ctxId, c.relation, c.lexicalType))

  def __repr__(self):
    return f"ContextTreeSection(roots={self.roots!r})"

  def __str__(self):
    return ("roots:"
            + ('\n' + '\n'.join(textwrap.indent(str(c), ' ')
                   for c in sorted(self.roots, key=lambda c: c.ctxId))
               if len(self.roots) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class Context(VersionedFormat,
    # Non-extendable structure
    _szChildren = (0, 0x00, 'Q'),
    _pChildren = (0, 0x08, 'Q'),
    ctxId = (0, 0x10, 'L'),
    _flags = (0, 0x14, 'B'),
    _relation = (0, 0x15, 'B'),
    _lexicalType = (0, 0x16, 'B'),
    _nFlexWords = (0, 0x17, 'B'),
  ):
  """Description for a calling (or otherwise) context."""

  __flags = VersionedBitFlags(
    # Added in v4.0
    hasFunction = (0, 0),
    hasSrcLoc = (0, 1),
    hasPoint = (0, 2),
  )
  @enum.unique
  class __Relation(enum.Enum):
    # Added in v4.0
    lexical = 0
    call = 1
    inlined_call = 2
  @enum.unique
  class __LexicalType(enum.Enum):
    # Added in v4.0
    function = 0
    loop = 1
    line = 2
    instruction = 3

  __slots__ = [
    '_lmTable', '_sfTable', '_fTable', '_children', 'flags',
    '_pFunction', '_pFile', 'line', '_pModule', 'offset',
    '_function', '_file', '_module',
  ]

  def _init_(self, *, children=[], ctxId, flags=set(), relation, lexicalType,
               function=None, file=None, line=None, module=None, offset=None):
    super()._init_()
    self.children = [c if isinstance(c, Context) else Context(**c)
                     for c in children]
    self.ctxId = ctxId.__index__()
    self.relation = relation
    self.lexicalType = lexicalType
    self.flags = self.__flags.normalize(float('inf'), flags)

    self.function = None
    if function is not None:
      self.function = function if isinstance(function, Function) else Function(**function)
      self.flags.add('hasFunction')

    self.file, self.line = None, None
    if file is not None:
      assert line is not None
      self.file = file if isinstance(file, SourceFile) else SourceFile(**file)
      self.line = line
      self.flags.add('hasSrcLoc')

    self.module, self.offset = None, None
    if module is not None:
      assert offset is not None
      self.module = module if isinstance(module, LoadModule) else LoadModule(**module)
      self.offset = offset
      self.flags.add('hasPoint')

  def unpack_from(self, version, src, /, offset=0, *args, **kwargs):
    super().unpack_from(version, src, offset, *args, **kwargs)
    self.flags = self.__flags.unpack(version, self._flags)

    words = []
    def add_subfield(code, name):
      sz = struct.calcsize(code)
      word, offset = None, None
      # Find a word with enough extra space for this one
      for w in words:
        if sum(b is not None for b in w) + sz <= 8:
          word = w
          # Find an appropriate run of bytes to allocate this to
          for guess in range(0, 8, sz):
            if all(b is None for b in w[guess:guess+sz]):
              offset = guess
              break
          assert offset is not None
          break
      if word is None:
        # Allocate a brand new word
        words.append([None]*8)
        word, offset = words[-1], 0

      # Allocate this space for this subfield
      word[offset] = (code, name)
      for i in range(offset+1, offset+sz): word[i] = True

    if 'hasFunction' in self.flags:
      add_subfield('Q', '_pFunction')
    else:
      self.function = None
    if 'hasSrcLoc' in self.flags:
      add_subfield('Q', '_pFile')
      add_subfield('L', 'line')
    else:
      self.file, self.line = None, None
    if 'hasPoint' in self.flags:
      add_subfield('Q', '_pModule')
      add_subfield('Q', 'offset')
    else:
      self.module, self.offset = None, None

    if len(words) > self._nFlexWords:
      raise ValueError(f"Attempt to unpack invalid Context, expected {len(words)} flex-words, got {self._nFlexWords}")

    # Stitch together the whole into a struct-format
    form, names = '<', []
    for w in words:
      for b in w:
        if b is None: form += 'x'
        elif b is True: pass  # Covered by a previous field
        else:
          code, name = b
          form += code
          names.append(name)
    for n,v in zip(names, unpack(struct.Struct(form), src, offset=offset+0x18)):
      setattr(self, n, v)

  @property
  def _total_size(self):
    return 0x18 + self._nFlexWords * 8

  @cached_property('_children')
  @VersionedFormat.subunpack(list)
  def children(self, *args):
    r = _parse_contexts(*args, offset=self._pChildren, size=self._szChildren)
    if hasattr(self, '_lmTable'):
      for c in r:
        c._lmTable, c._sfTable, c._fTable = self._lmTable, self._sfTable, self._fTable
    return r

  @property
  def relation(self): return self.__Relation(self._relation).name
  @relation.setter
  def relation(self, v): self._relation = self.__Relation[v].value
  @relation.deleter
  def relation(self): del self._relation

  @property
  def lexicalType(self): return self.__LexicalType(self._lexicalType).name
  @lexicalType.setter
  def lexicalType(self, v): self._lexicalType = self.__LexicalType[v].value
  @lexicalType.deleter
  def lexicalType(self): del self._lexicalType

  @cached_property('_function')
  @VersionedFormat.subunpack(lambda: None)
  def function(self, *args):
    if not hasattr(self, '_pFunction'): return None
    assert hasattr(self, '_fTable')
    return self._fTable[self._pFunction]

  @cached_property('_file')
  @VersionedFormat.subunpack(lambda: None)
  def file(self, *args):
    if not hasattr(self, '_pFile'): return None
    assert hasattr(self, '_sfTable')
    return self._sfTable[self._pFile]

  @property
  def srcloc(self):
    return (self.file, self.line) if self.file is not None else None

  @cached_property('_module')
  @VersionedFormat.subunpack(lambda: None)
  def module(self, *args):
    if not hasattr(self, '_pModule'): return None
    assert hasattr(self, '_lmTable')
    return self._lmTable[self._pModule]

  @property
  def point(self):
    return (self.module, self.offset) if self.module is not None else None

  def __eq__(self, other):
    if not isinstance(other, Context): return NotImplemented
    return (self.ctxId == other.ctxId and self.relation == other.relation
            and self.lexicalType == other.lexicalType and self.flags == other.flags
            and self.function == other.function and self.srcloc == other.srcloc
            and self.point == other.point and isomorphic_seq(
                self.children, other.children,
                key=lambda c: (c.ctxId, c.relation, c.lexicalType)))

  def __repr__(self):
    return (f"Context(ctxId={self.ctxId!r}, "
            f"relation={self.relation!r}, "
            f"lexicalType={self.lexicalType!r}, "
            f"flags={self.flags!r}, "
            f"function={self.function!r}, "
            f"file={self.file!r}, line={self.line!r}, "
            f"module={self.module!r}, offset={self.offset!r}, "
            f"children={self.children!r})")

  def __str__(self):
    return (f"-{self.relation}> {self.lexicalType} #{self.ctxId:d}:\n"
            + (f" function: {self.function.name}\n"
               + textwrap.indent(str(self.function), '  ') + "\n"
               if self.function is not None else "")
            + (f" srcloc: {os.path.basename(self.file.path)}:{self.line:d}\n"
               + textwrap.indent(str(self.file), '  ') + "\n"
               if self.file is not None else "")
            + (f" point: {os.path.basename(self.module.path)}+0x{self.offset:x}\n"
               + textwrap.indent(str(self.module), '  ') + "\n"
               if self.module is not None else "") +
            f" flags: {' '.join(str(s) for s in sorted(self.flags)) if len(self.flags) > 0 else '[]'}"
            + ('\n' + '\n'.join(textwrap.indent(str(c), '  ')
                   for c in sorted(self.children, key=lambda c: c.ctxId))
               if len(self.children) > 0 else "")
           )

  pack, pack_into = None, None
