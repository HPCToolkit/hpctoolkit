import dataclasses
import functools
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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class MetaDB(DatabaseFile):
    """The meta.db file format."""

    major_version = 4
    max_minor_version = 0
    format_code = b"meta"
    footer_code = b"_meta.db"
    yaml_tag: typing.ClassVar[str] = "!meta.db/v4"

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
    def kind_map(self) -> dict[int, str]:
        """Mapping from a kind index to the string name it represents."""
        return dict(enumerate(self.id_names.names))

    @functools.cached_property
    def raw_metric_map(self) -> dict[int, "PropagationScopeInstance"]:
        """Mapping from a statMetricId to the PropagationScopeInstance is represents."""
        return {i.prop_metric_id: i for m in self.metrics.metrics for i in m.scope_insts}

    @functools.cached_property
    def summary_metric_map(self) -> dict[int, "SummaryStatistic"]:
        """Mapping from a statMetricId to the SummaryStatistic is represents."""
        return {s.stat_metric_id: s for m in self.metrics.metrics for s in m.summaries}

    @functools.cached_property
    def context_map(self) -> dict[int, typing.Union[None, "EntryPoint", "Context"]]:
        """Mapping from a ctxId to the Context or EntryPoint it represents.

        Mapping to None is reserved for ctxId 0, which indicates the global root context.
        """
        result: dict[int, None | EntryPoint | Context] = {0: None}

        def traverse(ctx: "Context"):
            result[ctx.ctx_id] = ctx
            for c in ctx.children:
                traverse(c)

        for e in self.context.entry_points:
            result[e.ctx_id] = e
            for c in e.children:
                traverse(c)
        return result


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class GeneralProperties(StructureBase):
    """meta.db General Properties section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/GeneralProperties"

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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class IdentifierNames(StructureBase):
    """meta.db Hierarchical Identifier Names section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/IdentifierNames"

    names: list[str]

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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class PerformanceMetrics(StructureBase):
    """meta.db Performance Metrics section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/PerformanceMetrics"

    scopes: list["PropagationScope"]
    metrics: list["Metric"]

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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Metric(StructureBase):
    """Description for a single performance metric."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/Metric"

    name: str
    scope_insts: list["PropagationScopeInstance"]
    summaries: list["SummaryStatistic"]

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
        scopes_by_offset: dict[int, "PropagationScope"],
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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class PropagationScopeInstance(StructureBase):
    """Description of a single instantated propagation scope within a Metric."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/PropagationScopeInstance"

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
        cls, version: int, file, offset: int, scopes_by_offset: dict[int, "PropagationScope"]
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scope=scopes_by_offset[data["pScope"]],
            prop_metric_id=data["propMetricId"],
        )

    @classmethod
    def owning_fields(cls) -> tuple[dataclasses.Field, ...]:
        return tuple(f for f in dataclasses.fields(cls) if f.name not in ("scope",))


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class SummaryStatistic(StructureBase):
    """Description of a single summary static under a Scope."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/SummaryStatistic"

    @yaml_object
    class Combine(Enumeration, yaml_tag="!meta.db/v4/SummaryStatistic.Combine"):
        # Added in v4.0
        sum = EnumEntry(0, min_version=0)  # noqa: A003
        min = EnumEntry(1, min_version=0)  # noqa: A003
        max = EnumEntry(2, min_version=0)  # noqa: A003

    scope: "PropagationScope"
    formula: str
    combine: Combine | None
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
        cls, version: int, file, offset: int, scopes_by_offset: dict[int, "PropagationScope"]
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scope=scopes_by_offset[data["pScope"]],
            formula=read_ntstring(file, data["pFormula"]),
            combine=cls.Combine.versioned_decode(version, data["combine"]),
            stat_metric_id=data["statMetricId"],
        )

    @classmethod
    def owning_fields(cls) -> tuple[dataclasses.Field, ...]:
        return tuple(f for f in dataclasses.fields(cls) if f.name not in ("scope",))


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class PropagationScope(StructureBase):
    """Description of a single propagation scope."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/PropagationScope"

    @yaml_object
    class Type(Enumeration, yaml_tag="!meta.db/v4/PropagationScope.Type"):
        # Added in v4.0
        custom = EnumEntry(0, min_version=0)
        point = EnumEntry(1, min_version=0)
        execution = EnumEntry(2, min_version=0)
        transitive = EnumEntry(3, min_version=0)

    scope_name: str
    type: Type | None  # noqa: A003
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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class LoadModules(StructureBase):
    """meta.db Load Modules section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/LoadModules"

    modules: list["Module"]

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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Module(StructureBase):
    """Description for a single load module."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/Module"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!meta.db/v4/Module.Flags"):
        pass  # Reserved for future use

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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class SourceFiles(StructureBase):
    """meta.db Source Files section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/SourceFiles"

    files: list["File"]

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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class File(StructureBase):
    """Description for a single source file."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/File"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!meta.db/v4/File.Flags"):
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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Functions(StructureBase):
    """meta.db Functions section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/Functions"

    functions: list["Function"]

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
        modules_by_offset: dict[int, Module],
        files_by_offset: dict[int, File],
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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Function(StructureBase):
    """Description for a single performance metric."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/Function"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!meta.db/v4/Function.Flags"):
        pass  # Reserved for future use

    name: str
    module: Module | None
    offset: int | None
    file: File | None
    line: int | None
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
        modules_by_offset: dict[int, Module],
        files_by_offset: dict[int, File],
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
    def owning_fields(cls) -> tuple[dataclasses.Field, ...]:
        return tuple(f for f in dataclasses.fields(cls) if f.name not in ("module", "file"))


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ContextTree(StructureBase):
    """meta.db Context Tree section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/ContextTree"

    entry_points: list["EntryPoint"]

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
        modules_by_offset: dict[int, Module],
        files_by_offset: dict[int, File],
        functions_by_offset: dict[int, Function],
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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class EntryPoint(StructureBase):
    """meta.db Context Tree section."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/EntryPoint"

    @yaml_object
    class EntryPoint(Enumeration, yaml_tag="!meta.db/v4/EntryPoint.EntryPoint"):
        # Added in v4.0
        unknown_entry = EnumEntry(0, min_version=0)
        main_thread = EnumEntry(1, min_version=0)
        application_thread = EnumEntry(2, min_version=0)

    ctx_id: int
    entry_point: EntryPoint | None
    pretty_name: str
    children: list["Context"]

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
        modules_by_offset: dict[int, Module],
        files_by_offset: dict[int, File],
        functions_by_offset: dict[int, Function],
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


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Context(StructureBase):
    """Description for a calling (or otherwise) context."""

    yaml_tag: typing.ClassVar[str] = "!meta.db/v4/Context"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!meta.db/v4/Context.Flags"):
        # Added in v4.0
        has_function = EnumEntry(0, min_version=0)
        has_srcloc = EnumEntry(1, min_version=0)
        has_point = EnumEntry(2, min_version=0)

    @yaml_object
    class Relation(Enumeration, yaml_tag="!meta.db/v4/Context.Relation"):
        # Added in v4.0
        lexical = EnumEntry(0, min_version=0)
        call = EnumEntry(1, min_version=0)
        inlined_call = EnumEntry(2, min_version=0)

    @yaml_object
    class LexicalType(Enumeration, yaml_tag="!meta.db/v4/Context.LexicalType"):
        # Added in v4.0
        function = EnumEntry(0, min_version=0)
        loop = EnumEntry(1, min_version=0)
        line = EnumEntry(2, min_version=0)
        instruction = EnumEntry(3, min_version=0)

    ctx_id: int
    flags: Flags
    relation: Relation | None
    lexical_type: LexicalType | None
    propagation: int
    function: Function | None
    file: File | None
    line: int | None
    module: Module | None
    offset: int | None
    children: list["Context"]

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
        return f"-{r}> [{lt}] {' '.join(bits)}  #{self.ctx_id}"

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modules_by_offset: dict[int, Module],
        files_by_offset: dict[int, File],
        functions_by_offset: dict[int, Function],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        data["flags"] = cls.Flags.versioned_decode(version, data["flags"])

        words: list[tuple[bytes, dict[int, list[bool]]]] = []

        def mark_as_used(allocs, size, idx):
            for subsz in (8, 4, 2, 1):
                if subsz > size:
                    continue
                for i in range(idx * size // subsz, (idx + 1) * size // subsz):
                    allocs[subsz][i] = True

        def read_subfield(code):
            sz = struct.calcsize(code)
            assert sz in (8, 4, 2, 1)
            word, idx = None, None

            # If there's a word that has enough room to, use that
            for w in words:
                if not all(w[1][sz]):
                    word = w
                    idx = next(i for i, a in enumerate(word[1][sz]) if not a)
                    break
            else:
                # Otherwise add a new word and work from there
                word = (
                    read_nbytes(file, 8, offset + cls.__flex_offset + 8 * len(words)),
                    {8: [False], 4: [False] * 2, 2: [False] * 4, 1: [False] * 8},
                )
                words.append(word)
                idx = 0

            mark_as_used(word[1], sz, idx)
            return struct.unpack(code, word[0][sz * idx : sz * (idx + 1)])[0]

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
                children=cls.multi_from_file(
                    version,
                    file,
                    data["pChildren"],
                    data["szChildren"],
                    modules_by_offset=modules_by_offset,
                    files_by_offset=files_by_offset,
                    functions_by_offset=functions_by_offset,
                ),
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
        )

    @classmethod
    def multi_from_file(cls, version: int, file, start: int, size: int, **kwargs):
        result = []
        offset = start
        while offset < start + size:
            c, sz = cls.from_file(version, file, offset, **kwargs)
            result.append(c)
            offset += sz
        return result

    @classmethod
    def owning_fields(cls) -> tuple[dataclasses.Field, ...]:
        return tuple(
            f for f in dataclasses.fields(cls) if f.name not in ("function", "module", "file")
        )
