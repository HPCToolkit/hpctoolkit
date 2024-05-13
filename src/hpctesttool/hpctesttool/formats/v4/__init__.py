# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import dataclasses
import typing
from pathlib import Path

from ..base import DatabaseBase, yaml_object
from . import cctdb, metadb, profiledb, tracedb

__all__ = [
    "Database",
]


@yaml_object(yaml_tag="!db/v4")
@dataclasses.dataclass(eq=False)
class Database(DatabaseBase):
    """Top-level database object for major version 4 of the database formats."""

    major_version: typing.ClassVar[int] = 4

    meta: metadb.MetaDB
    profile: profiledb.ProfileDB
    context: cctdb.ContextDB
    trace: typing.Optional[tracedb.TraceDB]

    @classmethod
    def from_dir(cls, dbdir: Path):
        with open(dbdir / "meta.db", "rb") as metaf:
            meta = metadb.MetaDB.from_file(metaf)
        with open(dbdir / "profile.db", "rb") as profilef:
            profile = profiledb.ProfileDB.from_file(profilef)
            profile._with(meta)
        with open(dbdir / "cct.db", "rb") as contextf:
            context = cctdb.ContextDB.from_file(contextf)
            context._with(meta, profile)
        trace = None
        if (dbdir / "trace.db").is_file():
            with open(dbdir / "trace.db", "rb") as tracef:
                trace = tracedb.TraceDB.from_file(tracef)
                trace._with(meta, profile)
        return cls(meta=meta, profile=profile, context=context, trace=trace)

    def __setstate__(self, state):
        self.__dict__.update(state)
        self.profile._with(self.meta)
        self.context._with(self.meta, self.profile)
        if self.trace is not None:
            self.trace._with(self.meta, self.profile)

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
