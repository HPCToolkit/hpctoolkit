import dataclasses
import functools
import itertools
import struct
import typing

from .._util import VersionedStructure, read_nbytes, read_ntstring
from ..base import BitFlags, DatabaseFile, EnumEntry, Enumeration, StructureBase, yaml_object

__all__ = [
    "MetaDB",
    # v4.0
    "GeneralProperties",
    "IdentifierNames",
    "PerformanceMetrics",
    "Metric",
    "PropagationScopeInstance",
    "SummaryStatistic",
    "PropagationScope",
    "ContextTree",
    "EntryPoint",
    "Context",
    "LoadModules",
    "Module",
    "SourceFiles",
    "File",
    "Functions",
    "Function",
]


def scaled_range(start: int, cnt: int, step: int):
    for i in range(cnt):
        yield start + i * step


class _Flex:
    """Helper object to determine the offsets for a series of flex-fields."""

    def __init__(self) -> None:
        self._claimed: typing.Set[int] = set()

    def allocate(self, size: int) -> int:
        """Get the offset of the next flex-field in the packing order, with the given size."""
        if size not in (1, 2, 4, 8):
            raise ValueError(f"Unsupported size for flex-field: {size}")

        for trial in itertools.count(0, size):
            offsets = range(trial, trial + size)
            if not any(offset in self._claimed for offset in offsets):
                self._claimed |= set(offsets)
                return trial
        raise AssertionError

    def size(self) -> int:
        """Get the total span in bytes of the allocated flex-fields."""
        return max(self._claimed) + 1 if self._claimed else 0


@yaml_object(yaml_tag="!meta.db/v4")
@dataclasses.dataclass(eq=False)
class MetaDB(DatabaseFile):
    """The meta.db file format."""

    major_version = 4
    max_minor_version = 0
    format_code = b"meta"
    footer_code = b"_meta.db"

    general: "GeneralProperties"
    id_names: "IdentifierNames"
    metrics: "PerformanceMetrics"
    modules: "LoadModules"
    files: "SourceFiles"
    functions: "Functions"
    context: "ContextTree"

    __struct = DatabaseFile._header_struct(
        # Added in v4.0
        General=(0,),
        IdNames=(0,),
        Metrics=(0,),
        Context=(0,),
        Strings=(0,),
        Modules=(0,),
        Files=(0,),
        Functions=(0,),
    )

    @classmethod
    def from_file(cls, file):
        minor = cls._parse_header(file)
        sections = cls.__struct.unpack_file(minor, file, 0)
        modules, modules_by_offset = LoadModules.from_file(minor, file, sections["pModules"])
        files, files_by_offset = SourceFiles.from_file(minor, file, sections["pFiles"])
        functions, functions_by_offset = Functions.from_file(
            minor,
            file,
            sections["pFunctions"],
            modules_by_offset=modules_by_offset,
            files_by_offset=files_by_offset,
        )
        return cls(
            general=GeneralProperties.from_file(minor, file, sections["pGeneral"]),
            id_names=IdentifierNames.from_file(minor, file, sections["pIdNames"]),
            metrics=PerformanceMetrics.from_file(minor, file, sections["pMetrics"]),
            modules=modules,
            files=files,
            functions=functions,
            context=ContextTree.from_file(
                minor,
                file,
                sections["pContext"],
                modules_by_offset=modules_by_offset,
                files_by_offset=files_by_offset,
                functions_by_offset=functions_by_offset,
            ),
        )

    @functools.cached_property
    def kind_map(self) -> typing.Dict[int, str]:
        """Mapping from a kind index to the string name it represents."""
        return dict(enumerate(self.id_names.names))

    @functools.cached_property
    def raw_metric_map(self) -> typing.Dict[int, "PropagationScopeInstance"]:
        """Mapping from a statMetricId to the PropagationScopeInstance is represents."""
        return {i.prop_metric_id: i for m in self.metrics.metrics for i in m.scope_insts}

    @functools.cached_property
    def summary_metric_map(self) -> typing.Dict[int, "SummaryStatistic"]:
        """Mapping from a statMetricId to the SummaryStatistic is represents."""
        return {s.stat_metric_id: s for m in self.metrics.metrics for s in m.summaries}

    @functools.cached_property
    def context_map(self) -> typing.Dict[int, typing.Union[None, "EntryPoint", "Context"]]:
        """Mapping from a ctxId to the Context or EntryPoint it represents.

        Mapping to None is reserved for ctxId 0, which indicates the global root context.
        """
        result: typing.Dict[int, typing.Optional[typing.Union[EntryPoint, Context]]] = {0: None}

        def traverse(ctx: "Context"):
            result[ctx.ctx_id] = ctx
            for c in ctx.children:
                traverse(c)

        for e in self.context.entry_points:
            result[e.ctx_id] = e
            for c in e.children:
                traverse(c)
        return result


@yaml_object(yaml_tag="!meta.db/v4/GeneralProperties")
@dataclasses.dataclass(eq=False)
class GeneralProperties(StructureBase):
    """meta.db General Properties section."""

    title: str
    description: str

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pTitle=(0, 0x00, "Q"),
        pDescription=(0, 0x08, "Q"),
    )

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            title=read_ntstring(file, data["pTitle"]),
            description=read_ntstring(file, data["pDescription"]),
        )


@yaml_object(yaml_tag="!meta.db/v4/IdentifierNames")
@dataclasses.dataclass(eq=False)
class IdentifierNames(StructureBase):
    """meta.db Hierarchical Identifier Names section."""

    names: typing.List[str]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        ppNames=(0, 0x00, "Q"),
        nKinds=(0, 0x08, "B"),
    )
    __ptr = VersionedStructure(
        "<",
        ptr=(0, 0x00, "Q"),
    )

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            names=[
                read_ntstring(file, cls.__ptr.unpack_file(0, file, o)["ptr"])
                for o in scaled_range(data["ppNames"], data["nKinds"], cls.__ptr.size(0))
            ]
        )


@yaml_object(yaml_tag="!meta.db/v4/PerformanceMetrics")
@dataclasses.dataclass(eq=False)
class PerformanceMetrics(StructureBase):
    """meta.db Performance Metrics section."""

    scopes: typing.List["PropagationScope"]
    metrics: typing.List["Metric"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pMetrics=(0, 0x00, "Q"),
        nMetrics=(0, 0x08, "L"),
        szMetric=(0, 0x0C, "B"),
        szScopeInst=(0, 0x0D, "B"),
        szSummary=(0, 0x0E, "B"),
        pScopes=(0, 0x10, "Q"),
        nScopes=(0, 0x18, "H"),
        szScope=(0, 0x1A, "B"),
    )

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        scopes_by_off = {
            o: PropagationScope.from_file(version, file, o)
            for o in scaled_range(data["pScopes"], data["nScopes"], data["szScope"])
        }
        return cls(
            metrics=[
                Metric.from_file(
                    version,
                    file,
                    o,
                    scopes_by_offset=scopes_by_off,
                    sz_scope_inst=data["szScopeInst"],
                    sz_summary=data["szSummary"],
                )
                for o in scaled_range(data["pMetrics"], data["nMetrics"], data["szMetric"])
            ],
            scopes=list(scopes_by_off.values()),
        )


@yaml_object(yaml_tag="!meta.db/v4/Metric")
@dataclasses.dataclass(eq=False)
class Metric(StructureBase):
    """Description for a single performance metric."""

    name: str
    scope_insts: typing.List["PropagationScopeInstance"]
    summaries: typing.List["SummaryStatistic"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pName=(0, 0x00, "Q"),
        pScopeInsts=(0, 0x08, "Q"),
        pSummaries=(0, 0x10, "Q"),
        nScopeInsts=(0, 0x18, "H"),
        nSummaries=(0, 0x1A, "H"),
    )

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        scopes_by_offset: typing.Dict[int, "PropagationScope"],
        sz_scope_inst: int,
        sz_summary: int,
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            name=read_ntstring(file, data["pName"]),
            scope_insts=[
                PropagationScopeInstance.from_file(
                    version, file, o, scopes_by_offset=scopes_by_offset
                )
                for o in scaled_range(data["pScopeInsts"], data["nScopeInsts"], sz_scope_inst)
            ],
            summaries=[
                SummaryStatistic.from_file(version, file, o, scopes_by_offset=scopes_by_offset)
                for o in scaled_range(data["pSummaries"], data["nSummaries"], sz_summary)
            ],
        )


@yaml_object(yaml_tag="!meta.db/v4/PropagationScopeInstance")
@dataclasses.dataclass(eq=False)
class PropagationScopeInstance(StructureBase):
    """Description of a single instantated propagation scope within a Metric."""

    scope: "PropagationScope"
    prop_metric_id: int

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pScope=(0, 0x00, "Q"),
        propMetricId=(0, 0x08, "H"),
    )

    @property
    def shorthand(self) -> str:
        return f"{self.scope.scope_name}  #{self.prop_metric_id}"

    @classmethod
    def from_file(
        cls, version: int, file, offset: int, scopes_by_offset: typing.Dict[int, "PropagationScope"]
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scope=scopes_by_offset[data["pScope"]],
            prop_metric_id=data["propMetricId"],
        )

    @classmethod
    def owning_fields(cls) -> typing.Tuple[dataclasses.Field, ...]:
        return tuple(f for f in dataclasses.fields(cls) if f.name not in ("scope",))


@yaml_object(yaml_tag="!meta.db/v4/SummaryStatistic")
@dataclasses.dataclass(eq=False)
class SummaryStatistic(StructureBase):
    """Description of a single summary static under a Scope."""

    @yaml_object(yaml_tag="!meta.db/v4/SummaryStatistic.Combine")
    class Combine(Enumeration):
        # Added in v4.0
        sum = EnumEntry(0, min_version=0)
        min = EnumEntry(1, min_version=0)
        max = EnumEntry(2, min_version=0)

    scope: "PropagationScope"
    formula: str
    combine: typing.Optional[Combine]
    stat_metric_id: int

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pScope=(0, 0x00, "Q"),
        pFormula=(0, 0x08, "Q"),
        combine=(0, 0x10, "B"),
        statMetricId=(0, 0x12, "H"),
    )

    @property
    def shorthand(self) -> str:
        cb = self.combine.name if self.combine is not None else "<unknown>"
        return f"{cb} / '{self.formula}' / {self.scope.scope_name}  #{self.stat_metric_id}"

    @classmethod
    def from_file(
        cls, version: int, file, offset: int, scopes_by_offset: typing.Dict[int, "PropagationScope"]
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scope=scopes_by_offset[data["pScope"]],
            formula=read_ntstring(file, data["pFormula"]),
            combine=cls.Combine.versioned_decode(version, data["combine"]),
            stat_metric_id=data["statMetricId"],
        )

    @classmethod
    def owning_fields(cls) -> typing.Tuple[dataclasses.Field, ...]:
        return tuple(f for f in dataclasses.fields(cls) if f.name not in ("scope",))


@yaml_object(yaml_tag="!meta.db/v4/PropagationScope")
@dataclasses.dataclass(eq=False)
class PropagationScope(StructureBase):
    """Description of a single propagation scope."""

    @yaml_object(yaml_tag="!meta.db/v4/PropagationScope.Type")
    class Type(Enumeration):
        # Added in v4.0
        custom = EnumEntry(0, min_version=0)
        point = EnumEntry(1, min_version=0)
        execution = EnumEntry(2, min_version=0)
        transitive = EnumEntry(3, min_version=0)

    scope_name: str
    type: typing.Optional[Type]
    propagation_index: int

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pScopeName=(0, 0x00, "Q"),
        type=(0, 0x08, "B"),
        propagationIndex=(0, 0x09, "B"),
    )

    @property
    def shorthand(self) -> str:
        suffix = ""
        if self.type == self.Type.transitive:
            suffix = f" bit/{self.propagation_index}"
        ty = self.type.name if self.type is not None else "<unknown>"
        return f"{self.scope_name} [{ty}]" + suffix

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scope_name=read_ntstring(file, data["pScopeName"]),
            type=cls.Type.versioned_decode(version, data["type"]),
            propagation_index=data["propagationIndex"],
        )


@yaml_object(yaml_tag="!meta.db/v4/LoadModules")
@dataclasses.dataclass(eq=False)
class LoadModules(StructureBase):
    """meta.db Load Modules section."""

    modules: typing.List["Module"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pModules=(0, 0x00, "Q"),
        nModules=(0, 0x08, "L"),
        szModule=(0, 0x0C, "H"),
    )

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        modules_by_offset = {
            o: Module.from_file(version, file, o)
            for o in scaled_range(data["pModules"], data["nModules"], data["szModule"])
        }
        return (
            cls(
                modules=list(modules_by_offset.values()),
            ),
            modules_by_offset,
        )


@yaml_object(yaml_tag="!meta.db/v4/Module")
@dataclasses.dataclass(eq=False)
class Module(StructureBase):
    """Description for a single load module."""

    @yaml_object(yaml_tag="!meta.db/v4/Module.Flags")
    class Flags(BitFlags):
        # Flag bits reserved for future use. This entry may be removed once
        # bits are allocated.
        RESERVED = EnumEntry(33, min_version=0x100)

    if typing.TYPE_CHECKING:
        _ = Flags.RESERVED

    flags: Flags
    path: str

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        flags=(0, 0x00, "L"),
        pPath=(0, 0x08, "Q"),
    )

    @property
    def shortpath(self) -> str:
        return self.path[self.path.rfind("/") :] if "/" in self.path else self.path

    @property
    def shorthand(self) -> str:
        return f"{self.shortpath} [{self.flags.name or ''}]"

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            flags=cls.Flags.versioned_decode(version, data["flags"]),
            path=read_ntstring(file, data["pPath"]),
        )


@yaml_object(yaml_tag="!meta.db/v4/SourceFiles")
@dataclasses.dataclass(eq=False)
class SourceFiles(StructureBase):
    """meta.db Source Files section."""

    files: typing.List["File"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pFiles=(0, 0x00, "Q"),
        nFiles=(0, 0x08, "L"),
        szFile=(0, 0x0C, "H"),
    )

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        files_by_offset = {
            o: File.from_file(version, file, o)
            for o in scaled_range(data["pFiles"], data["nFiles"], data["szFile"])
        }
        return cls(files=list(files_by_offset.values())), files_by_offset


@yaml_object(yaml_tag="!meta.db/v4/File")
@dataclasses.dataclass(eq=False)
class File(StructureBase):
    """Description for a single source file."""

    @yaml_object(yaml_tag="!meta.db/v4/File.Flags")
    class Flags(BitFlags):
        # Added in v4.0
        copied = EnumEntry(0, min_version=0)

    flags: Flags
    path: str

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        flags=(0, 0x00, "L"),
        pPath=(0, 0x08, "Q"),
    )

    @property
    def shortpath(self) -> str:
        return self.path[self.path.rfind("/") :] if "/" in self.path else self.path

    @property
    def shorthand(self) -> str:
        return f"{self.shortpath} [{self.flags.name or ''}]"

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            flags=cls.Flags.versioned_decode(version, data["flags"]),
            path=read_ntstring(file, data["pPath"]),
        )


@yaml_object(yaml_tag="!meta.db/v4/Functions")
@dataclasses.dataclass(eq=False)
class Functions(StructureBase):
    """meta.db Functions section."""

    functions: typing.List["Function"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pFunctions=(0, 0x00, "Q"),
        nFunctions=(0, 0x08, "L"),
        szFunction=(0, 0x0C, "H"),
    )

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modules_by_offset: typing.Dict[int, Module],
        files_by_offset: typing.Dict[int, File],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        functions_by_offset = {
            o: Function.from_file(
                version,
                file,
                o,
                modules_by_offset=modules_by_offset,
                files_by_offset=files_by_offset,
            )
            for o in scaled_range(data["pFunctions"], data["nFunctions"], data["szFunction"])
        }
        return cls(functions=list(functions_by_offset.values())), functions_by_offset


@yaml_object(yaml_tag="!meta.db/v4/Function")
@dataclasses.dataclass(eq=False)
class Function(StructureBase):
    """Description for a single performance metric."""

    @yaml_object(yaml_tag="!meta.db/v4/Function.Flags")
    class Flags(BitFlags):
        # Flag bits reserved for future use. This entry may be removed once
        # bits are allocated.
        RESERVED = EnumEntry(33, min_version=0x100)

    if typing.TYPE_CHECKING:
        _ = Flags.RESERVED

    name: str
    module: typing.Optional[Module]
    offset: typing.Optional[int]
    file: typing.Optional[File]
    line: typing.Optional[int]
    flags: Flags

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pName=(0, 0x00, "Q"),
        pModule=(0, 0x08, "Q"),
        offset=(0, 0x10, "Q"),
        pFile=(0, 0x18, "Q"),
        line=(0, 0x20, "L"),
        flags=(0, 0x24, "L"),
    )

    def __post_init__(self):
        if self.module is None:
            self.offset = None
        elif self.offset is None:
            raise ValueError("offset cannot be None if module is not None!")

        if self.file is None:
            self.line = None
        elif self.line is None:
            raise ValueError("line cannot be None if file is not None!")

    @property
    def shorthand(self) -> str:
        srcloc = f" {self.file.shortpath}:{self.line}" if self.file is not None else ""
        point = f" {self.module.shortpath}+0x{self.offset:x}" if self.module is not None else ""
        return f"{self.name}{srcloc}{point} [{self.flags.name or ''}]"

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modules_by_offset: typing.Dict[int, Module],
        files_by_offset: typing.Dict[int, File],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            name=read_ntstring(file, data["pName"]),
            module=modules_by_offset[data["pModule"]] if data["pModule"] != 0 else None,
            offset=data["offset"],
            file=files_by_offset[data["pFile"]] if data["pFile"] != 0 else None,
            line=data["line"],
            flags=cls.Flags.versioned_decode(version, data["flags"]),
        )

    @classmethod
    def owning_fields(cls) -> typing.Tuple[dataclasses.Field, ...]:
        return tuple(f for f in dataclasses.fields(cls) if f.name not in ("module", "file"))


@yaml_object(yaml_tag="!meta.db/v4/ContextTree")
@dataclasses.dataclass(eq=False)
class ContextTree(StructureBase):
    """meta.db Context Tree section."""

    entry_points: typing.List["EntryPoint"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pEntryPoints=(0, 0x00, "Q"),
        nEntryPoints=(0, 0x08, "H"),
        szEntryPoint=(0, 0x0A, "B"),
    )

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modules_by_offset: typing.Dict[int, Module],
        files_by_offset: typing.Dict[int, File],
        functions_by_offset: typing.Dict[int, Function],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            entry_points=[
                EntryPoint.from_file(
                    version,
                    file,
                    o,
                    modules_by_offset=modules_by_offset,
                    files_by_offset=files_by_offset,
                    functions_by_offset=functions_by_offset,
                )
                for o in scaled_range(
                    data["pEntryPoints"], data["nEntryPoints"], data["szEntryPoint"]
                )
            ]
        )


@yaml_object(yaml_tag="!meta.db/v4/EntryPoint")
@dataclasses.dataclass(eq=False)
class EntryPoint(StructureBase):
    """meta.db Context Tree section."""

    @yaml_object(yaml_tag="!meta.db/v4/EntryPoint.EntryPoint")
    class EntryPoint(Enumeration):
        # Added in v4.0
        unknown_entry = EnumEntry(0, min_version=0)
        main_thread = EnumEntry(1, min_version=0)
        application_thread = EnumEntry(2, min_version=0)

    ctx_id: int
    entry_point: typing.Optional[EntryPoint]
    pretty_name: str
    children: typing.List["Context"]

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        szChildren=(0, 0x00, "Q"),
        pChildren=(0, 0x08, "Q"),
        ctxId=(0, 0x10, "L"),
        entryPoint=(0, 0x14, "H"),
        pPrettyName=(0, 0x18, "Q"),
    )

    @property
    def shorthand(self) -> str:
        ep = self.entry_point.name if self.entry_point is not None else "<unknown>"
        return f"{self.pretty_name} (= {ep})  #{self.ctx_id}"

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modules_by_offset: typing.Dict[int, Module],
        files_by_offset: typing.Dict[int, File],
        functions_by_offset: typing.Dict[int, Function],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            children=Context.multi_from_file(
                version,
                file,
                data["pChildren"],
                data["szChildren"],
                modules_by_offset=modules_by_offset,
                files_by_offset=files_by_offset,
                functions_by_offset=functions_by_offset,
            ),
            ctx_id=data["ctxId"],
            entry_point=cls.EntryPoint.versioned_decode(version, data["entryPoint"]),
            pretty_name=read_ntstring(file, data["pPrettyName"]),
        )


@yaml_object(yaml_tag="!meta.db/v4/Context")
@dataclasses.dataclass(eq=False)
class Context(StructureBase):
    """Description for a calling (or otherwise) context."""

    @yaml_object(yaml_tag="!meta.db/v4/Context.Flags")
    class Flags(BitFlags):
        # Added in v4.0
        has_function = EnumEntry(0, min_version=0)
        has_srcloc = EnumEntry(1, min_version=0)
        has_point = EnumEntry(2, min_version=0)

    @yaml_object(yaml_tag="!meta.db/v4/Context.Relation")
    class Relation(Enumeration):
        # Added in v4.0
        lexical = EnumEntry(0, min_version=0)
        call = EnumEntry(1, min_version=0)
        inlined_call = EnumEntry(2, min_version=0)

    @yaml_object(yaml_tag="!meta.db/v4/Context.LexicalType")
    class LexicalType(Enumeration):
        # Added in v4.0
        function = EnumEntry(0, min_version=0)
        loop = EnumEntry(1, min_version=0)
        line = EnumEntry(2, min_version=0)
        instruction = EnumEntry(3, min_version=0)

    ctx_id: int
    flags: Flags
    relation: typing.Optional[Relation]
    lexical_type: typing.Optional[LexicalType]
    propagation: int
    function: typing.Optional[Function]
    file: typing.Optional[File]
    line: typing.Optional[int]
    module: typing.Optional[Module]
    offset: typing.Optional[int]
    children: typing.List["Context"]

    __struct = VersionedStructure(
        "<",
        # Fixed-length header, flexible part comes after
        szChildren=(0, 0x00, "Q"),
        pChildren=(0, 0x08, "Q"),
        ctxId=(0, 0x10, "L"),
        flags=(0, 0x14, "B"),
        relation=(0, 0x15, "B"),
        lexicalType=(0, 0x16, "B"),
        nFlexWords=(0, 0x17, "B"),
        propagation=(0, 0x18, "H"),
    )
    __flex_offset = 0x20
    assert __struct.size(float("inf")) <= __flex_offset

    def __post_init__(self):
        if self.Flags.has_function in self.flags:
            if self.function is None:
                raise ValueError("hasFunction implies function is not None!")
        elif self.function is not None:
            raise ValueError("!hasFunction implies function is None!")

        if self.Flags.has_srcloc in self.flags:
            if self.file is None or self.line is None:
                raise ValueError("hasSrcLoc implies file and line are not None!")
        elif self.file is not None or self.line is not None:
            raise ValueError("!hasSrcLoc implies file and line are None!")

        if self.Flags.has_point in self.flags:
            if self.module is None or self.offset is None:
                raise ValueError("hasPoint implies module and offset are not None!")
        elif self.module is not None or self.offset is not None:
            raise ValueError("!hasPoint implies module and offset are None!")

    @property
    def shorthand(self) -> str:
        bits = []
        if self.Flags.has_function in self.flags:
            assert self.function is not None
            bits.append(self.function.name)
        if self.Flags.has_srcloc in self.flags:
            assert self.file is not None
            assert self.line is not None
            bits.append(f"{self.file.shortpath}:{self.line:d}")
        if self.Flags.has_point in self.flags:
            assert self.module is not None
            assert self.offset is not None
            bits.append(f"{self.module.shortpath}+0x{self.offset:x}")
        r = self.relation.name if self.relation is not None else "<unknown>"
        lt = self.lexical_type.name if self.lexical_type is not None else "<unknown>"
        ms = [i for i in range(16) if self.propagation & (1 << i) != 0]
        p = f" ^M[{','.join(map(str, ms))}]" if ms else ""
        return f"-{r}> [{lt}] {' '.join(bits)}  #{self.ctx_id}{p}"

    @classmethod
    def _from_file(
        cls,
        version: int,
        file,
        offset: int,
        modules_by_offset: typing.Dict[int, Module],
        files_by_offset: typing.Dict[int, File],
        functions_by_offset: typing.Dict[int, Function],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        data["flags"] = cls.Flags.versioned_decode(version, data["flags"])

        flex = _Flex()

        def read_subfield(code):
            sz = struct.calcsize(code)
            assert sz in (8, 4, 2, 1)
            field_off = flex.allocate(sz)
            return struct.unpack(
                code, read_nbytes(file, sz, offset + cls.__flex_offset + field_off)
            )[0]

        data["function"] = None
        if cls.Flags.has_function in data["flags"]:
            data["function"] = functions_by_offset[read_subfield("<Q")]

        data["file"] = data["line"] = None
        if cls.Flags.has_srcloc in data["flags"]:
            data["file"] = files_by_offset[read_subfield("<Q")]
            data["line"] = read_subfield("<L")

        data["module"] = data["offset"] = None
        if cls.Flags.has_point in data["flags"]:
            data["module"] = modules_by_offset[read_subfield("<Q")]
            data["offset"] = read_subfield("<Q")

        return (
            cls(
                children=[],
                ctx_id=data["ctxId"],
                flags=data["flags"],
                relation=cls.Relation.versioned_decode(version, data["relation"]),
                lexical_type=cls.LexicalType.versioned_decode(version, data["lexicalType"]),
                propagation=data["propagation"],
                function=data["function"],
                file=data["file"],
                line=data["line"],
                module=data["module"],
                offset=data["offset"],
            ),
            cls.__flex_offset + data["nFlexWords"] * 8,
            data["pChildren"],
            data["szChildren"],
        )

    @classmethod
    def from_file(cls, version: int, file, offset: int, **kwargs):
        result, sz, pchild, szchild = cls._from_file(version, file, offset, **kwargs)
        result.children = cls.multi_from_file(version, file, pchild, szchild, **kwargs)
        return result, sz

    @classmethod
    def multi_from_file(cls, version: int, file, start: int, size: int, **kwargs):
        result: typing.List[Context] = []
        q: typing.List[typing.Tuple[int, int, typing.List[Context]]] = [(start, size, result)]
        while q:
            start, size, target = q.pop()
            offset = start
            while offset < start + size:
                c, sz, pchild, szchild = cls._from_file(version, file, offset, **kwargs)
                target.append(c)
                c.children = []
                q.append((pchild, szchild, c.children))
                offset += sz

        return result

    @classmethod
    def owning_fields(cls) -> typing.Tuple[dataclasses.Field, ...]:
        return tuple(
            f for f in dataclasses.fields(cls) if f.name not in ("function", "module", "file")
        )
