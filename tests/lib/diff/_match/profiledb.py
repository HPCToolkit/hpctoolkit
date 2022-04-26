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

from .common import *
from ...formats.v4 import MetaDB
from ...formats.v4.profiledb import *

from functools import singledispatch

@singledispatch
def match(a, b): raise NotImplementedError

@match.register
def match_profiledb(a: ProfileDB, b: ProfileDB, /, *,
                    metadb: MatchResult) -> MatchResult:
  """
  Compare two ProfileDB objects and find similarities. See match().
  """
  check_tyb(b, ProfileDB)
  check_tym(metadb, MetaDB)
  out = MatchResult.matchif(a, b, a.minorVersion == b.minorVersion)
  out |= match_profilesSec(a.profiles, b.profiles, metadb=metadb)
  return out

@match.register
def match_profilesSec(a: ProfileInfoSection, b: ProfileInfoSection, /, *,
                      metadb: MatchResult) -> MatchResult:
  check_tyb(b, ProfileInfoSection)
  check_tym(metadb, MetaDB)
  f = lambda a, b: match_profile(a, b, metadb=metadb)
  return (MatchResult.match(a, b)
          | merge_unordered(a.profiles, b.profiles, match=match_profile))

@match.register
def match_profile(a: ProfileInfo, b: ProfileInfo, /, *,
                  metadb: MatchResult) -> MatchResult:
  check_tyb(b, ProfileInfo)
  check_tym(metadb, MetaDB)
  out = MatchResult.matchif(a, b, a.idTuple == b.idTuple)
  if out:
    # Also add matches within the ValueBlock
    out.matches[a.valueBlock] = b.valueBlock

  return out
