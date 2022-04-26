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
from ..._util import read_ntstring, array_unpack, cached_property

from struct import Struct

@VersionedFormat.minimize
class IdentifierNamesSection(VersionedFormat,
    # Added in v4.0
    _ppNames = (0, 0x00, 'Q'),
    _nKinds = (0, 0x08, 'B'),
  ):
  """meta.db Hierarchical Identifier Names section."""
  __slots__ = ['_names']
  __ptr = Struct('<Q')

  def _init_(self, *, names=[]):
    super()._init_()
    self.names = list(names)

  @cached_property('_names')
  @VersionedFormat.subunpack(list)
  def names(self, version, src):
    return [read_ntstring(src, p) for p, in
            array_unpack(self.__ptr, self._nKinds, src, self._ppNames)]

  def identical(self, other):
    if not isinstance(other, IdentifierNamesSection): raise TypeError(type(other))
    return self.names == other.names

  def __repr__(self):
    return f"{self.__class__.__name__}(names={self.names!r})"

  def __str__(self):
    return ("names:"
            + ('\n' + '\n'.join(' - ' + n for n in self.names)
               if hasattr(self, 'names') else " []")
           )

  pack, pack_into = None, None
