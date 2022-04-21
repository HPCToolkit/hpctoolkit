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
from ..._util import cached_property, read_ntstring

import textwrap

@VersionedFormat.minimize
class GeneralPropertiesSection(VersionedFormat,
    # Added in v4.0
    _pTitle = (0, 0x00, 'Q'),
    _pDescription = (0, 0x08, 'Q'),
  ):
  """meta.db General Properties section."""
  __slots__ = ['title', 'description']

  def _init_(self, *, title, description):
    super()._init_()
    self.title = title
    self.description = description

  def unpack_from(self, version, src, **kwargs):
    super().unpack_from(version, src, **kwargs)
    self.title = read_ntstring(src, offset=self._pTitle)
    self.description = read_ntstring(src, offset=self._pDescription)

  def __eq__(self, other):
    if not isinstance(other, GeneralPropertiesSection): return NotImplemented
    return self.title == other.title and self.description == other.description

  def __repr__(self):
    return (f"{self.__class__.__name__}(title={self.title!r}, "
            f"description={self.description!r})")

  def __str__(self):
    return (f"title: {self.title}\n"
            f"description:"
            + ('\n' + textwrap.indent(self.description, '  ')
               if hasattr(self, 'description') else " None")
           )

  pack, pack_into = None, None
