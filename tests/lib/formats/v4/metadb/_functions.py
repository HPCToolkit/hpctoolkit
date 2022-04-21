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
from ..._util import (read_ntstring, cached_property, VersionedBitFlags,
                      isomorphic_seq)

from ._load_modules import LoadModulesSection, LoadModule
from ._source_files import SourceFilesSection, SourceFile

import enum
import os.path
import textwrap

@VersionedFormat.minimize
class FunctionsSection(VersionedFormat,
    # Added in v4.0
    _pFunctions = (0, 0x00, 'Q'),
    _nFunctions = (0, 0x08, 'L'),
    _szFunction = (0, 0x0c, 'H'),
  ):
  """meta.db Functions section."""
  __slots__ = ['_lmTable', '_sfTable', '_functions']

  def _init_(self, *, functions=[]):
    super()._init_()
    self.functions = [f if isinstance(f, Function) else Function(**f)
                      for f in functions]

  def unpack_from(self, version, src, /, *args, lmSec=None, sfSec=None, **kwargs):
    if not isinstance(lmSec, LoadModulesSection):
      raise TypeError(f"lmSec must be a LoadModulesSection, got {type(lmSec)}")
    if not isinstance(sfSec, SourceFilesSection):
      raise TypeError(f"sfSec must be a SourceFilesSection, got {type(sfSec)}")
    self._lmTable = {m._srcoffset: m for m in lmSec.modules if hasattr(m, '_srcoffset')}
    self._sfTable = {f._srcoffset: f for f in sfSec.files if hasattr(f, '_srcoffset')}
    super().unpack_from(version, src, *args, **kwargs)

  @cached_property('_functions')
  @VersionedFormat.subunpack(list)
  def functions(self, *args):
    r = [Function(*args, offset=self._pFunctions + i*self._szFunction)
         for i in range(self._nFunctions)]
    for f in r: f._lmTable, f._sfTable = self._lmTable, self._sfTable
    return r

  def __eq__(self, other):
    if not isinstance(other, FunctionsSection): return NotImplemented
    return isomorphic_seq(self.functions, other.functions,
                          key=lambda f: f.name)

  def __repr__(self):
    return f"FunctionsSection(functions={self.functions!r})"

  def __str__(self):
    return ("functions:"
            + ('\n' + '\n'.join(' - ' + textwrap.indent(str(m), '   ')[3:]
                   for m in sorted(self.functions, key=lambda f: f.name))
               if len(self.functions) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class Function(VersionedFormat,
    # Added in v4.0
    _pName = (0, 0x00, 'Q'),
    _pModule = (0, 0x08, 'Q'),
    offset = (0, 0x10, 'Q'),
    _pFile = (0, 0x18, 'Q'),
    line = (0, 0x20, 'L'),
    _flags = (0, 0x24, 'L'),
  ):
  """Description for a single performance metric."""
  __flags = VersionedBitFlags()
  __slots__ = ['_lmTable', '_sfTable', 'name', '_module', '_file', 'flags', '_srcoffset']

  def _init_(self, *, name, flags=set(), module=None, offset=None,
               file=None, line=None):
    super()._init_()
    self.name = name
    self.flags = self.__flags.normalize(float('inf'), flags)

    self.module, self.offset = None, None
    if module is not None:
      assert offset is not None
      self.module = module if isinstance(module, LoadModule) else LoadModule(**module)
      self.offset = offset.__index__()

    self.file, self.line = None, None
    if file is not None:
      assert line is not None
      self.file = file if isinstance(file, SourceFile) else SourceFile(**file)
      self.line = line.__index__()

  def unpack_from(self, version, src, offset=0, **kwargs):
    super().unpack_from(version, src, offset=offset, **kwargs)
    self._srcoffset = offset
    self.name = read_ntstring(src, self._pName)
    self.flags = self.__flags.unpack(version, self._flags)

  @cached_property('_module')
  @VersionedFormat.subunpack(lambda: None)
  def module(self, *args):
    if self._pModule == 0: return None
    return self._lmTable[self._pModule]

  @property
  def point(self):
    return (self.module, self.offset) if self.module is not None else None

  @cached_property('_file')
  @VersionedFormat.subunpack(lambda: None)
  def file(self, *args):
    if self._pFile == 0: return None
    return self._sfTable[self._pFile]

  @property
  def srcloc(self):
    return (self.file, self.line) if self.file is not None else None

  def __eq__(self, other):
    if not isinstance(other, Function): return NotImplemented
    return self.name == other.name and self.point == other.point \
           and self.srcloc == other.srcloc and self.flags == other.flags

  def __repr__(self):
    return (f"Function(name={self.name!r}, "
            f"module={self.module!r}, offset={self.offset!r}, "
            f"file={self.file!r}, line={self.line!r}, "
            f"flags={self.flags!r})")

  def __str__(self):
    return (f"name: {self.name}\n"
            f"module:"
            + (f" {os.path.basename(self.module.path)}+0x{self.offset:x}\n"
               + textwrap.indent(str(self.module), '  ')
               if self.module is not None else " None") + "\n"
            f"file:"
            + (f" {os.path.basename(self.file.path)}:{self.line:d}\n"
               + textwrap.indent(str(self.file), '  ')
               if self.file is not None else " None") + "\n"
            f"flags:"
            + ('\n' + '\n'.join(' - '+textwrap.indent(str(s), '   ')[3:]
                                for s in sorted(self.flags))
               if len(self.flags) > 0 else " []")
           )

  pack, pack_into = None, None
