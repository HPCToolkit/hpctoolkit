import dataclasses
import typing

from .._util import VersionedStructure
from ..base import DatabaseFile, StructureBase, _CommentedMap, yaml_object

if typing.TYPE_CHECKING:
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


@yaml_object(yaml_tag="!cct.db/v4")
@dataclasses.dataclass(eq=False)
class ContextDB(DatabaseFile):
    """The cct.db file format."""

    major_version = 4
    max_minor_version = 0
    format_code = b"ctxt"
    footer_code = b"__ctx.db"

    ctx_infos: "ContextInfos"

    __struct = DatabaseFile._header_struct(
        # Added in v4.0
        CtxInfos=(0,),
    )

    def _with(self, meta: "MetaDB", profile: "ProfileDB"):
        self.ctx_infos._with(meta, profile)

    @classmethod
    def from_file(cls, file):
        minor = cls._parse_header(file)
        sections = cls.__struct.unpack_file(minor, file, 0)
        return cls(
            ctx_infos=ContextInfos.from_file(minor, file, sections["pCtxInfos"]),
        )


@yaml_object(yaml_tag="!cct.db/v4/ContextInfos")
@dataclasses.dataclass(eq=False)
class ContextInfos(StructureBase):
    """Section containing header metadata for the contexts in this database."""

    contexts: typing.List["PerContext"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pCtxs=(0, 0x00, "Q"),
        nCtxs=(0, 0x08, "L"),
        szCtx=(0, 0x0C, "B"),
    )

    def __post_init__(self):
        for ctx_id, c in enumerate(self.contexts):
            c._with_id(ctx_id)

    def __setstate__(self, state):
        self.__dict__.update(state)
        for ctx_id, c in enumerate(self.contexts):
            c._with_id(ctx_id)

    def _with(self, meta: "MetaDB", profile: "ProfileDB"):
        for ctx_id, c in enumerate(self.contexts):
            c._with(meta, profile, ctx_id)

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            contexts=[
                PerContext.from_file(version, file, o)
                for o in scaled_range(data["pCtxs"], data["nCtxs"], data["szCtx"])
            ],
        )


@yaml_object(yaml_tag="!cct.db/v4/PerContext")
@dataclasses.dataclass(eq=False)
class PerContext(StructureBase):
    """Information on a single context."""

    values: typing.Dict[int, typing.Dict[int, float]]

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

    def _with_id(self, ctx_id: int):
        self._ctx_id = ctx_id

    def _with(self, meta: "MetaDB", profile: "ProfileDB", ctx_id: int):
        self._ctx_id = ctx_id
        if self._ctx_id in meta.context_map:
            self._context = meta.context_map[self._ctx_id]
        self._profile_map = profile.profile_map
        self._metric_map = meta.raw_metric_map

    @property
    def shorthand(self) -> typing.Optional[str]:
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
        met_indices = [
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
                            met_indices[i + 1]["startIndex"] if i + 1 < len(met_indices) else None
                        )
                    ]
                }
                for i, idx in enumerate(met_indices)
            }
        )

    @classmethod
    def to_yaml(cls, representer, obj):
        state = cls._as_commented_map(obj)

        if hasattr(obj, "_profile_map") and hasattr(obj, "_metric_map"):
            p_map, m_map = obj._profile_map, obj._metric_map

            state["values"] = _CommentedMap(state["values"])
            for met_id in state["values"]:
                if met_id in m_map:
                    state["values"].yaml_add_eol_comment(
                        "for " + m_map[met_id].shorthand,
                        key=met_id,
                    )
                state["values"][met_id] = _CommentedMap(state["values"][met_id])
                for prof_idx in state["values"][met_id]:
                    if prof_idx in p_map:
                        com = (
                            p_map[prof_idx].id_tuple.shorthand
                            if p_map[prof_idx].id_tuple is not None
                            else "/"
                        )
                        state["values"][met_id].yaml_add_eol_comment(
                            f"for {{{com}}}",
                            key=prof_idx,
                        )

        return representer.represent_mapping(cls.yaml_tag, state)
