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
import functools
import typing as T

from .._util import VersionedStructure
from ..base import BitFlags, DatabaseFile, StructureBase, _CommentedMap, yaml_object

if T.TYPE_CHECKING:
    from .metadb import MetaDB

__all__ = [
    "ProfileDB",
    # v4.0
    "ProfileInfos",
    "Profile",
    "IdentifierTuple",
    "Identifier",
]


def scaled_range(start: int, cnt: int, step: int):
    for i in range(cnt):
        yield start + i * step


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ProfileDB(DatabaseFile):
    """The profile.db file format."""

    major_version = 4
    max_minor_version = 0
    format_code = b"prof"
    footer_code = b"_prof.db"
    yaml_tag: T.ClassVar[str] = "!profile.db/v4"

    ProfileInfos: "ProfileInfos"

    __struct = DatabaseFile._header_struct(
        # Added in v4.0
        ProfileInfos=(0,),
        IdTuples=(0,),
    )

    def _with(self, meta: "MetaDB"):
        self.ProfileInfos._with(meta)  # noqa: protected-access

    @classmethod
    def from_file(cls, file):
        minor = cls._parse_header(file)
        sections = cls.__struct.unpack_file(minor, file, 0)
        return cls(
            ProfileInfos=ProfileInfos.from_file(minor, file, sections["pProfileInfos"]),
        )

    @functools.cached_property
    def profile_map(self) -> dict[int, "Profile"]:
        """Mapping from a profIndex to the Profile it represents."""
        return dict(enumerate(self.ProfileInfos.profiles))


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ProfileInfos(StructureBase):
    """Section containing header metadata for the profiles in this database."""

    yaml_tag: T.ClassVar[str] = "!profile.db/v4/ProfileInfos"

    profiles: list["Profile"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pProfiles=(0, 0x00, "Q"),
        nProfiles=(0, 0x08, "L"),
        szProfile=(0, 0x0C, "B"),
    )

    def _with(self, meta: "MetaDB"):
        for p in self.profiles:
            p._with(meta)  # noqa: protected-access

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            profiles=[
                Profile.from_file(version, file, data["pProfiles"] + data["szProfile"] * i)
                for i in range(data["nProfiles"])
            ]
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Profile(StructureBase):
    """Information on a single profile."""

    yaml_tag: T.ClassVar[str] = "!profile.db/v4/Profile"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!profile.db/v4/Profile.Flags"):
        # Added in v4.0
        isSummary = (0, 0)  # noqa: N815

    idTuple: T.Optional["IdentifierTuple"]  # noqa: N815
    flags: Flags
    values: dict[int, dict[int, float]]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        valueBlock_nValues=(0, 0x00, "Q"),
        valueBlock_pValues=(0, 0x08, "Q"),
        valueBlock_nCtxs=(0, 0x10, "L"),
        valueBlock_pCtxIndices=(0, 0x18, "Q"),
        pIdTuple=(0, 0x20, "Q"),
        flags=(0, 0x28, "L"),
    )

    __value = VersionedStructure(
        "<",
        # Fixed structure
        metricId=(-1, 0x00, "H"),
        value=(-1, 0x02, "d"),
    )
    assert __value.size(0) == 0x0A
    __ctx_idx = VersionedStructure(
        "<",
        # Fixed structure
        ctxId=(-1, 0x00, "L"),
        startIndex=(-1, 0x04, "Q"),
    )
    assert __ctx_idx.size(0) == 0x0C

    @property
    def shorthand(self) -> str:
        return f"{{{self.idTuple.shorthand if self.idTuple else '/'}}} [{' '.join(e.name for e in self.Flags if e in self.flags)}]"

    def _with(self, meta: "MetaDB"):
        if self.idTuple is not None:
            self.idTuple._with(meta)  # noqa: protected-access
        self._context_map = meta.context_map
        self._metric_map = (
            meta.summary_metric_map if self.Flags.isSummary in self.flags else meta.raw_metric_map
        )

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        values = [
            cls.__value.unpack_file(0, file, o)
            for o in scaled_range(
                data["valueBlock_pValues"], data["valueBlock_nValues"], cls.__value.size(0)
            )
        ]
        ctxIndices = [
            cls.__ctx_idx.unpack_file(0, file, o)
            for o in scaled_range(
                data["valueBlock_pCtxIndices"], data["valueBlock_nCtxs"], cls.__ctx_idx.size(0)
            )
        ]
        return cls(
            idTuple=IdentifierTuple.from_file(version, file, data["pIdTuple"])
            if data["pIdTuple"] != 0
            else None,
            flags=cls.Flags.versioned_decode(version, data["flags"]),
            values={
                idx["ctxId"]: {
                    val["metricId"]: val["value"]
                    for val in values[
                        idx["startIndex"] : (
                            ctxIndices[i + 1]["startIndex"] if i + 1 < len(ctxIndices) else None
                        )
                    ]
                }
                for i, idx in enumerate(ctxIndices)
            },
        )

    @classmethod
    def to_yaml(cls, representer, obj):
        state = cls._as_commented_map(obj)

        if hasattr(obj, "_context_map") and hasattr(obj, "_metric_map"):
            c_map, m_map = obj._context_map, obj._metric_map  # noqa: protected-access

            state["values"] = _CommentedMap(state["values"])
            for ctxId in state["values"]:
                if ctxId in c_map:
                    com = c_map[ctxId].shorthand if c_map[ctxId] is not None else "<root>"
                    state["values"].yaml_add_eol_comment("for " + com, key=ctxId)
                state["values"][ctxId] = _CommentedMap(state["values"][ctxId])
                for metId in state["values"][ctxId]:
                    if metId in m_map:
                        state["values"][ctxId].yaml_add_eol_comment(
                            "for " + m_map[metId].shorthand, key=metId
                        )

        return representer.represent_mapping(cls.yaml_tag, state)


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class IdentifierTuple(StructureBase):
    """Single hierarchical identifier tuple."""

    yaml_tag: T.ClassVar[str] = "!profile.db/v4/IdentifierTuple"

    ids: list["Identifier"]

    # NB: Although this structure is extendable, its total size is fixed
    size: T.ClassVar[int] = 0x08
    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        nIds=(0, 0x00, "H"),
    )
    assert __struct.size(float("inf")) <= size

    @property
    def shorthand(self) -> str:
        return " / ".join(i.shorthand for i in self.ids)

    def _with(self, meta: "MetaDB"):
        for i in self.ids:
            i._with(meta)  # noqa: protected-access

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            ids=[
                Identifier.from_file(version, file, o)
                for o in scaled_range(offset + cls.size, data["nIds"], Identifier.size)
            ],
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Identifier(StructureBase):
    yaml_tag: T.ClassVar[str] = "!profile.db/v4/Identifier"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!profile.db/v4/Identifier.Flags"):
        # Added in v4.0
        isPhysical = (0, 0)  # noqa: N815

    kind: int
    flags: Flags
    logicalId: int  # noqa: N815
    physicalId: int  # noqa: N815

    # NB: Although this structure is extendable, its total size is fixed
    size: T.ClassVar[int] = 0x10
    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        kind=(0, 0x00, "B"),
        flags=(0, 0x02, "H"),
        logicalId=(0, 0x04, "L"),
        physicalId=(0, 0x08, "Q"),
    )
    assert __struct.size(float("inf")) == size

    @property
    def shorthand(self) -> str:
        kind = self._kindstr if hasattr(self, "_kindstr") else f"[{self.kind:d}]"
        suffix = ""
        if self.Flags.isPhysical in self.flags:
            suffix = f" [0x{self.physicalId:x}]"
        return f"{kind} {self.logicalId:d}" + suffix

    def _with(self, meta: "MetaDB"):
        self._kindstr = meta.kind_map[self.kind]

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return Identifier(
            kind=data["kind"],
            flags=cls.Flags.versioned_decode(version, data["flags"]),
            logicalId=data["logicalId"],
            physicalId=data["physicalId"],
        )
