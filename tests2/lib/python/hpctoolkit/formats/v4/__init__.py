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

import dataclasses
import typing as T
from pathlib import Path

import ruamel.yaml

from ..base import DatabaseBase, yaml_object
from . import cctdb, metadb, profiledb, tracedb

__all__ = [
    "Database",
]


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Database(DatabaseBase):
    """Top-level database object for major version 4 of the database formats."""

    major_version: T.ClassVar[int] = 4
    yaml_tag: T.ClassVar[str] = "!db/v4"

    meta: metadb.MetaDB
    profile: profiledb.ProfileDB
    context: cctdb.ContextDB
    trace: tracedb.TraceDB | None

    @classmethod
    def from_dir(cls, dbdir: Path):
        with open(dbdir / "meta.db", "rb") as metaf:
            meta = metadb.MetaDB.from_file(metaf)
        with open(dbdir / "profile.db", "rb") as profilef:
            profile = profiledb.ProfileDB.from_file(profilef)
            profile._with(meta)  # noqa: protected-access
        with open(dbdir / "cct.db", "rb") as contextf:
            context = cctdb.ContextDB.from_file(contextf)
            context._with(meta, profile)  # noqa: protected-access
        trace = None
        if (dbdir / "trace.db").is_file():
            with open(dbdir / "trace.db", "rb") as tracef:
                trace = tracedb.TraceDB.from_file(tracef)
                trace._with(meta, profile)  # noqa: protected-access
        return cls(meta=meta, profile=profile, context=context, trace=trace)

    def __setstate__(self, state):
        self.__dict__.update(state)
        self.profile._with(self.meta)  # noqa: protected-access
        self.context._with(self.meta, self.profile)  # noqa: protected-access
        if self.trace is not None:
            self.trace._with(self.meta, self.profile)  # noqa: protected-access

    @classmethod
    def register_yaml(cls, yaml: ruamel.yaml.YAML) -> ruamel.yaml.YAML:
        yaml.register_class(cls)
        metadb.MetaDB.register_yaml(yaml)
        profiledb.ProfileDB.register_yaml(yaml)
        cctdb.ContextDB.register_yaml(yaml)
        tracedb.TraceDB.register_yaml(yaml)
        return yaml

    @property
    def kind_map(self):
        return self.meta.kind_map

    @property
    def raw_metric_map(self):
        return self.meta.raw_metric_map

    @property
    def summary_metric_map(self):
        return self.meta.summary_metric_map

    @property
    def context_map(self):
        return self.meta.context_map

    @property
    def profile_map(self):
        return self.profile.profile_map
