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

import enum
import textwrap

@VersionedFormat.minimize
class LoadModulesSection(VersionedFormat,
    # Added in v4.0
    _pModules = (0, 0x00, 'Q'),
    _nModules = (0, 0x08, 'L'),
    _szModule = (0, 0x0c, 'H'),
  ):
  """meta.db Load Modules section."""
  __slots__ = ['_modules']

  def _init_(self, *, modules=[]):
    super()._init_()
    self.modules = {m if isinstance(m, LoadModule) else LoadModule(**m)
                    for m in modules}

  @cached_property('_modules')
  @VersionedFormat.subunpack(set)
  def modules(self, *args):
    return {LoadModule(*args, offset=self._pModules + i*self._szModule)
            for i in range(self._nModules)}

  def identical(self, other):
    if not isinstance(other, LoadModulesSection): raise TypeError(type(other))
    return isomorphic_seq(self.modules, other.modules,
                          lambda a,b: a.identical(b), key=lambda m: m.path)

  def __repr__(self):
    return f"LoadModulesSection(modules={self.modules!r})"

  def __str__(self):
    return ("modules:"
            + ('\n' + '\n'.join(' - ' + textwrap.indent(str(m), '   ')[3:]
                   for m in sorted(self.modules, key=lambda m: m.path))
               if len(self.modules) > 0 else " []")
           )

  pack, pack_into = None, None


@VersionedFormat.minimize
class LoadModule(VersionedFormat,
    # Added in v4.0
    _flags = (0, 0x00, 'L'),
    _pPath = (0, 0x08, 'Q'),
  ):
  """Description for a single load module."""
  __flags = VersionedBitFlags()
  __slots__ = ['path', 'flags', '_srcoffset']

  def _init_(self, *, path, flags=set()):
    super()._init_()
    self.path = path
    self.flags = self.__flags.normalize(float('inf'), flags)

  def unpack_from(self, version, src, /, *args, **kwargs):
    super().unpack_from(version, src, *args, **kwargs)
    self._srcoffset = int(kwargs.get('offset', 0))
    self.path = read_ntstring(src, self._pPath)
    self.flags = self.__flags.unpack(version, self._flags)

  def identical(self, other):
    if not isinstance(other, LoadModule): raise TypeError(type(other))
    return self.path == other.path and self.flags == other.flags

  def __repr__(self):
    return (f"LoadModule(path={self.path!r}, flags={self.flags!r})")

  def __str__(self):
    return (f"path: {self.path}\n"
            f"flags:"
            + ('\n' + '\n'.join(' - '+textwrap.indent(str(s), '   ')[3:]
                                for s in sorted(self.flags))
               if len(self.flags) > 0 else " []")
           )

  pack, pack_into = None, None
