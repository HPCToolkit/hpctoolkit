# SPDX-FileCopyrightText: 2022-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

import dataclasses
import functools
import typing

from .._util import VersionedStructure
from ..base import BitFlags, DatabaseFile, EnumEntry, StructureBase, _CommentedMap, yaml_object

if typing.TYPE_CHECKING:
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


@yaml_object(yaml_tag="!profile.db/v4")
@dataclasses.dataclass(eq=False)
class ProfileDB(DatabaseFile):
    """The profile.db file format."""

    major_version = 4
    max_minor_version = 0
    format_code = b"prof"
    footer_code = b"_prof.db"

    profile_infos: "ProfileInfos"

    __struct = DatabaseFile._header_struct(
        # Added in v4.0
        ProfileInfos=(0,),
        IdTuples=(0,),
    )

    def _with(self, meta: "MetaDB"):
        self.profile_infos._with(meta)

    @classmethod
    def from_file(cls, file):
        minor = cls._parse_header(file)
        sections = cls.__struct.unpack_file(minor, file, 0)
        return cls(
            profile_infos=ProfileInfos.from_file(minor, file, sections["pProfileInfos"]),
        )

    @functools.cached_property
    def profile_map(self) -> typing.Dict[int, "Profile"]:
        """Mapping from a profIndex to the Profile it represents."""
        return dict(enumerate(self.profile_infos.profiles))


@yaml_object(yaml_tag="!profile.db/v4/ProfileInfos")
@dataclasses.dataclass(eq=False)
class ProfileInfos(StructureBase):
    """Section containing header metadata for the profiles in this database."""

    profiles: typing.List["Profile"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pProfiles=(0, 0x00, "Q"),
        nProfiles=(0, 0x08, "L"),
        szProfile=(0, 0x0C, "B"),
    )

    def _with(self, meta: "MetaDB"):
        for p in self.profiles:
            p._with(meta)

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            profiles=[
                Profile.from_file(version, file, data["pProfiles"] + data["szProfile"] * i)
                for i in range(data["nProfiles"])
            ]
        )


@yaml_object(yaml_tag="!profile.db/v4/Profile")
@dataclasses.dataclass(eq=False)
class Profile(StructureBase):
    """Information on a single profile."""

    @yaml_object(yaml_tag="!profile.db/v4/Profile.Flags")
    class Flags(BitFlags):
        # Added in v4.0
        is_summary = EnumEntry(0, min_version=0)

    id_tuple: typing.Optional["IdentifierTuple"]
    flags: Flags
    values: typing.Dict[int, typing.Dict[int, float]]

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
        return f"{{{self.id_tuple.shorthand if self.id_tuple else '/'}}} [{self.flags.name or ''}]"

    def _with(self, meta: "MetaDB"):
        if self.id_tuple is not None:
            self.id_tuple._with(meta)
        self._context_map = meta.context_map
        self._metric_map = (
            meta.summary_metric_map if self.Flags.is_summary in self.flags else meta.raw_metric_map
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
        ctx_indices = [
            cls.__ctx_idx.unpack_file(0, file, o)
            for o in scaled_range(
                data["valueBlock_pCtxIndices"], data["valueBlock_nCtxs"], cls.__ctx_idx.size(0)
            )
        ]
        return cls(
            id_tuple=(
                IdentifierTuple.from_file(version, file, data["pIdTuple"])
                if data["pIdTuple"] != 0
                else None
            ),
            flags=cls.Flags.versioned_decode(version, data["flags"]),
            values={
                idx["ctxId"]: {
                    val["metricId"]: val["value"]
                    for val in values[
                        idx["startIndex"] : (
                            ctx_indices[i + 1]["startIndex"] if i + 1 < len(ctx_indices) else None
                        )
                    ]
                }
                for i, idx in enumerate(ctx_indices)
            },
        )

    @classmethod
    def to_yaml(cls, representer, obj):
        state = cls._as_commented_map(obj)

        if hasattr(obj, "_context_map") and hasattr(obj, "_metric_map"):
            c_map, m_map = obj._context_map, obj._metric_map

            state["values"] = _CommentedMap(state["values"])
            for ctx_id in state["values"]:
                if ctx_id in c_map:
                    com = c_map[ctx_id].shorthand if c_map[ctx_id] is not None else "<root>"
                    state["values"].yaml_add_eol_comment("for " + com, key=ctx_id)
                state["values"][ctx_id] = _CommentedMap(state["values"][ctx_id])
                for met_id in state["values"][ctx_id]:
                    if met_id in m_map:
                        state["values"][ctx_id].yaml_add_eol_comment(
                            "for " + m_map[met_id].shorthand, key=met_id
                        )

        return representer.represent_mapping(cls.yaml_tag, state)


@yaml_object(yaml_tag="!profile.db/v4/IdentifierTuple")
@dataclasses.dataclass(eq=False)
class IdentifierTuple(StructureBase):
    """Single hierarchical identifier tuple."""

    ids: typing.List["Identifier"]

    # NB: Although this structure is extendable, its total size is fixed
    size: typing.ClassVar[int] = 0x08
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
            i._with(meta)

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            ids=[
                Identifier.from_file(version, file, o)
                for o in scaled_range(offset + cls.size, data["nIds"], Identifier.size)
            ],
        )


@yaml_object(yaml_tag="!profile.db/v4/Identifier")
@dataclasses.dataclass(eq=False)
class Identifier(StructureBase):
    @yaml_object(yaml_tag="!profile.db/v4/Identifier.Flags")
    class Flags(BitFlags):
        # Added in v4.0
        is_physical = EnumEntry(0, min_version=0)

    kind: int
    flags: Flags
    logical_id: int
    physical_id: int

    # NB: Although this structure is extendable, its total size is fixed
    size: typing.ClassVar[int] = 0x10
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
        if self.Flags.is_physical in self.flags:
            suffix = f" [0x{self.physical_id:x}]"
        return f"{kind} {self.logical_id:d}" + suffix

    def _with(self, meta: "MetaDB"):
        self._kindstr = meta.kind_map[self.kind]

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return Identifier(
            kind=data["kind"],
            flags=cls.Flags.versioned_decode(version, data["flags"]),
            logical_id=data["logicalId"],
            physical_id=data["physicalId"],
        )
