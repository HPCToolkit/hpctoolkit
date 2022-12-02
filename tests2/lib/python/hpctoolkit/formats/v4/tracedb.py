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

from .._util import VersionedStructure
from ..base import DatabaseFile, StructureBase, yaml_object

if T.TYPE_CHECKING:
    from .metadb import MetaDB
    from .profiledb import ProfileDB

__all__ = [
    "TraceDB",
    # v4.0
    "ContextTraceHeadersSection",
    "ContextTrace",
    "ContextTraceElement",
]


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class TraceDB(DatabaseFile):
    """The trace.db file format."""

    major_version = 4
    max_minor_version = 0
    format_code = b"trce"
    footer_code = b"trace.db"
    yaml_tag: T.ClassVar[str] = "!trace.db/v4"

    CtxTraces: "ContextTraceHeadersSection"

    __struct = DatabaseFile._header_struct(
        # Added in v4.0
        CtxTraces=(0,),
    )

    def _with(self, meta: "MetaDB", profile: "ProfileDB"):
        self.CtxTraces._with(meta, profile)

    @classmethod
    def from_file(cls, file):
        minor = cls._parse_header(file)
        sections = cls.__struct.unpack_file(minor, file, 0)
        return cls(
            CtxTraces=ContextTraceHeadersSection.from_file(minor, file, sections["pCtxTraces"]),
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ContextTraceHeadersSection(StructureBase):
    """trace.db Context Trace Headers section."""

    yaml_tag: T.ClassVar[str] = "!trace.db/v4/ContextTraceHeaders"

    timestampRange: T.TypedDict("TimestampMinMax", {"min": int, "max": int})
    traces: list["ContextTrace"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pTraces=(0, 0x00, "Q"),
        nTraces=(0, 0x08, "L"),
        szTrace=(0, 0x0C, "B"),
        minTimestamp=(0, 0x10, "Q"),
        maxTimestamp=(0, 0x18, "Q"),
    )

    def _with(self, meta: "MetaDB", profile: "ProfileDB"):
        for t in self.traces:
            t._with(meta, profile)

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            traces=[
                ContextTrace.from_file(version, file, data["pTraces"] + data["szTrace"] * i)
                for i in range(data["nTraces"])
            ],
            timestampRange={"min": data["minTimestamp"], "max": data["maxTimestamp"]},
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ContextTrace(StructureBase):
    """Header for a single trace of Contexts."""

    yaml_tag: T.ClassVar[str] = "!trace.db/v4/ContextTrace"

    profIndex: int
    line: list["ContextTraceElement"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        profIndex=(0, 0x00, "L"),
        pStart=(0, 0x08, "Q"),
        pEnd=(0, 0x10, "Q"),
    )

    @property
    def shorthand(self) -> str:
        if len(self.line) == 0:
            tot = "0s"
            rang = "0-0"
        else:
            tot = f"{(self.line[-1].timestamp-self.line[0].timestamp) / 1000000000:.9f}s"
            rang = (
                f"{self.line[0].timestamp/1000000000:.9f}-{self.line[-1].timestamp/1000000000:.9f}"
            )
        prof = (
            f"{{{self._profile.idTuple.shorthand}}}"
            if hasattr(self, "_profile")
            else f"[{self.profIndex}]"
        )
        return f"{tot} ({rang}) for {prof}"

    def __post_init__(self):
        for e in self.line:
            e._with_first(self.line[0].timestamp)

    def __setstate__(self, state):
        self.__dict__.update(state)
        for e in self.line:
            e._with_first(self.line[0].timestamp)

    def _with(self, meta: "MetaDB", profile: "ProfileDB"):
        if self.profIndex in profile.profile_map:
            self._profile = profile.profile_map[self.profIndex]
        for e in self.line:
            e._with(meta)

    @classmethod
    def from_file(cls, version, file, offset):
        data = cls.__struct.unpack_file(version, file, offset)
        line = [
            ContextTraceElement.from_file(file, o)
            for o in range(data["pStart"], data["pEnd"], ContextTraceElement.size)
        ]
        return cls(
            profIndex=data["profIndex"],
            line=line,
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ContextTraceElement(StructureBase):
    yaml_tag: T.ClassVar[str] = "!trace.db/v4/ContextTraceElement"

    timestamp: int
    ctxId: int

    __struct = VersionedStructure(
        # Fixed structure
        "<",
        timestamp=(-1, 0x00, "Q"),
        ctxId=(-1, 0x08, "L"),
    )
    size: T.ClassVar[int] = __struct.size(0)

    @property
    def shorthand(self) -> str:
        tim = (
            f"+{(self.timestamp - self._first) / 1000000000:.9f}s"
            if hasattr(self, "_first")
            else f"{self.timestamp/1000000000:.9f}s"
        )
        if hasattr(self, "_context"):
            ctx = self._context.shorthand if self._context is not None else "<root>"
        else:
            ctx = f"[{self.ctxId}]"
        return f"{tim} at {ctx}"

    def _with_first(self, first: int):
        self._first = first

    def _with(self, meta: "MetaDB"):
        if self.ctxId in meta.context_map:
            self._context = meta.context_map[self.ctxId]

    @classmethod
    def from_file(cls, file, offset):
        data = cls.__struct.unpack_file(0, file, offset)
        return cls(timestamp=data["timestamp"], ctxId=data["ctxId"])
