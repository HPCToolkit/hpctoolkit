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
from ..base import DatabaseFile, StructureBase, _CommentedMap, yaml_object

if T.TYPE_CHECKING:
    from .metadb import MetaDB
    from .profiledb import ProfileDB

__all__ = [
    "ContextDB",
    # v4.0
    "ContextInfos",
    "PerContext",
]


def scaled_range(start: int, cnt: int, step: int):
    for i in range(cnt):
        yield start + i * step


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ContextDB(DatabaseFile):
    """The cct.db file format."""

    major_version = 4
    max_minor_version = 0
    format_code = b"ctxt"
    footer_code = b"__ctx.db"
    yaml_tag: T.ClassVar[str] = "!cct.db/v4"

    CtxInfos: "ContextInfos"

    __struct = DatabaseFile._header_struct(
        # Added in v4.0
        CtxInfos=(0,),
    )

    def _with(self, meta: "MetaDB", profile: "ProfileDB"):
        self.CtxInfos._with(meta, profile)

    @classmethod
    def from_file(cls, file):
        minor = cls._parse_header(file)
        sections = cls.__struct.unpack_file(minor, file, 0)
        return cls(
            CtxInfos=ContextInfos.from_file(minor, file, sections["pCtxInfos"]),
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ContextInfos(StructureBase):
    """Section containing header metadata for the contexts in this database."""

    yaml_tag: T.ClassVar[str] = "!cct.db/v4/ContextInfos"

    contexts: list["PerContext"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pCtxs=(0, 0x00, "Q"),
        nCtxs=(0, 0x08, "L"),
        szCtx=(0, 0x0C, "B"),
    )

    def __post_init__(self):
        for ctxId, c in enumerate(self.contexts):
            c._with_id(ctxId)

    def __setstate__(self, state):
        self.__dict__.update(state)
        for ctxId, c in enumerate(self.contexts):
            c._with_id(ctxId)

    def _with(self, meta: "MetaDB", profile: "ProfileDB"):
        for ctxId, c in enumerate(self.contexts):
            c._with(meta, profile, ctxId)

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            contexts=[
                PerContext.from_file(version, file, o)
                for o in scaled_range(data["pCtxs"], data["nCtxs"], data["szCtx"])
            ],
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class PerContext(StructureBase):
    """Information on a single context."""

    yaml_tag: T.ClassVar[str] = "!cct.db/v4/PerContext"

    values: dict[int, dict[int, float]]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        valueBlock_nValues=(0, 0x00, "Q"),
        valueBlock_pValues=(0, 0x08, "Q"),
        valueBlock_nMetrics=(0, 0x10, "H"),
        valueBlock_pMetricIndices=(0, 0x18, "Q"),
    )

    __value = VersionedStructure(
        "<",
        # Fixed structure
        profIndex=(-1, 0x00, "L"),
        value=(-1, 0x04, "d"),
    )
    assert __value.size(0) == 0x0C
    __met_idx = VersionedStructure(
        "<",
        # Fixed structure
        metricId=(-1, 0x00, "H"),
        startIndex=(-1, 0x02, "Q"),
    )
    assert __met_idx.size(0) == 0x0A

    def _with_id(self, ctxId: int):
        self._ctx_id = ctxId

    def _with(self, meta: "MetaDB", profile: "ProfileDB", ctxId: int):
        self._ctx_id = ctxId
        if self._ctx_id in meta.context_map:
            self._context = meta.context_map[self._ctx_id]
        self._profile_map = profile.profile_map
        self._metric_map = meta.raw_metric_map

    @property
    def shorthand(self) -> str | None:
        if hasattr(self, "_context"):
            return "for " + (self._context.shorthand if self._context is not None else "<root>")
        if hasattr(self, "_ctx_id"):
            return f"for [{self._ctx_id:d}]"
        return None

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        values = [
            cls.__value.unpack_file(0, file, o)
            for o in scaled_range(
                data["valueBlock_pValues"], data["valueBlock_nValues"], cls.__value.size(0)
            )
        ]
        metIndices = [
            cls.__met_idx.unpack_file(0, file, o)
            for o in scaled_range(
                data["valueBlock_pMetricIndices"],
                data["valueBlock_nMetrics"],
                cls.__met_idx.size(0),
            )
        ]
        return cls(
            values={
                idx["metricId"]: {
                    val["profIndex"]: val["value"]
                    for val in values[
                        idx["startIndex"] : (
                            metIndices[i + 1]["startIndex"] if i + 1 < len(metIndices) else None
                        )
                    ]
                }
                for i, idx in enumerate(metIndices)
            }
        )

    @classmethod
    def to_yaml(cls, representer, obj):
        state = cls._as_commented_map(obj)

        if hasattr(obj, "_profile_map") and hasattr(obj, "_metric_map"):
            p_map, m_map = obj._profile_map, obj._metric_map

            state["values"] = _CommentedMap(state["values"])
            for metId in state["values"]:
                if metId in m_map:
                    state["values"].yaml_add_eol_comment(
                        "for " + m_map[metId].shorthand,
                        key=metId,
                    )
                state["values"][metId] = _CommentedMap(state["values"][metId])
                for profIdx in state["values"][metId]:
                    if profIdx in p_map:
                        com = (
                            p_map[profIdx].idTuple.shorthand
                            if p_map[profIdx].idTuple is not None
                            else "/"
                        )
                        state["values"][metId].yaml_add_eol_comment(
                            f"for {{{com}}}",
                            key=profIdx,
                        )

        return representer.represent_mapping(cls.yaml_tag, state)
