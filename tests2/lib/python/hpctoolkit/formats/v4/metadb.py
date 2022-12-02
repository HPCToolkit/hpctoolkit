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
import struct
import typing as T

from .._util import VersionedStructure, read_nbytes, read_ntstring
from ..base import BitFlags, DatabaseFile, Enumeration, StructureBase, yaml_object

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
    yaml_tag: T.ClassVar[str] = "!meta.db/v4"

    General: "GeneralProperties"
    IdNames: "IdentifierNames"
    Metrics: "PerformanceMetrics"
    Modules: "LoadModules"
    Files: "SourceFiles"
    Functions: "Functions"
    Context: "ContextTree"

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
        modules, modulesByOffset = LoadModules.from_file(minor, file, sections["pModules"])
        files, filesByOffset = SourceFiles.from_file(minor, file, sections["pFiles"])
        functions, functionsByOffset = Functions.from_file(
            minor,
            file,
            sections["pFunctions"],
            modulesByOffset=modulesByOffset,
            filesByOffset=filesByOffset,
        )
        return cls(
            General=GeneralProperties.from_file(minor, file, sections["pGeneral"]),
            IdNames=IdentifierNames.from_file(minor, file, sections["pIdNames"]),
            Metrics=PerformanceMetrics.from_file(minor, file, sections["pMetrics"]),
            Modules=modules,
            Files=files,
            Functions=functions,
            Context=ContextTree.from_file(
                minor,
                file,
                sections["pContext"],
                modulesByOffset=modulesByOffset,
                filesByOffset=filesByOffset,
                functionsByOffset=functionsByOffset,
            ),
        )

    @functools.cached_property
    def kind_map(self) -> dict[int, str]:
        """Mapping from a kind index to the string name it represents."""
        return dict(enumerate(self.IdNames.names))

    @functools.cached_property
    def raw_metric_map(self) -> dict[int, "PropagationScopeInstance"]:
        """Mapping from a statMetricId to the PropagationScopeInstance is represents."""
        return {i.propMetricId: i for m in self.Metrics.metrics for i in m.scopeInsts}

    @functools.cached_property
    def summary_metric_map(self) -> dict[int, "SummaryStatistic"]:
        """Mapping from a statMetricId to the SummaryStatistic is represents."""
        return {s.statMetricId: s for m in self.Metrics.metrics for s in m.summaries}

    @functools.cached_property
    def context_map(self) -> dict[int, T.Union[None, "EntryPoint", "Context"]]:
        """Mapping from a ctxId to the Context or EntryPoint it represents.

        Mapping to None is reserved for ctxId 0, which indicates the global root context."""
        result = {0: None}

        def traverse(ctx: "Context"):
            result[ctx.ctxId] = ctx
            for c in ctx.children:
                traverse(c)

        for e in self.Context.entryPoints:
            result[e.ctxId] = e
            for c in e.children:
                traverse(c)
        return result


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class GeneralProperties(StructureBase):
    """meta.db General Properties section."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/GeneralProperties"

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

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/IdentifierNames"

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

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/PerformanceMetrics"

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
        scopesByOff = {
            o: PropagationScope.from_file(version, file, o)
            for o in scaled_range(data["pScopes"], data["nScopes"], data["szScope"])
        }
        return cls(
            metrics=[
                Metric.from_file(
                    version,
                    file,
                    o,
                    scopesByOffset=scopesByOff,
                    szScopeInst=data["szScopeInst"],
                    szSummary=data["szSummary"],
                )
                for o in scaled_range(data["pMetrics"], data["nMetrics"], data["szMetric"])
            ],
            scopes=list(scopesByOff.values()),
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Metric(StructureBase):
    """Description for a single performance metric."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/Metric"

    name: str
    scopeInsts: list["PropagationScopeInstance"]
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
        scopesByOffset: dict[int, "PropagationScope"],
        szScopeInst: int,
        szSummary: int,
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            name=read_ntstring(file, data["pName"]),
            scopeInsts=[
                PropagationScopeInstance.from_file(version, file, o, scopesByOffset=scopesByOffset)
                for o in scaled_range(data["pScopeInsts"], data["nScopeInsts"], szScopeInst)
            ],
            summaries=[
                SummaryStatistic.from_file(version, file, o, scopesByOffset=scopesByOffset)
                for o in scaled_range(data["pSummaries"], data["nSummaries"], szSummary)
            ],
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class PropagationScopeInstance(StructureBase):
    """Description of a single instantated propagation scope within a Metric."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/PropagationScopeInstance"

    scope: "PropagationScope"
    propMetricId: int

    __struct = VersionedStructure(
        "<",
        # Added in v4.0
        pScope=(0, 0x00, "Q"),
        propMetricId=(0, 0x08, "H"),
    )

    @property
    def shorthand(self) -> str:
        return f"{self.scope.scopeName}  #{self.propMetricId}"

    @classmethod
    def from_file(
        cls, version: int, file, offset: int, scopesByOffset: dict[int, "PropagationScope"]
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scope=scopesByOffset[data["pScope"]],
            propMetricId=data["propMetricId"],
        )

    @classmethod
    def owning_fields(cls) -> list[dataclasses.Field]:
        return [f for f in dataclasses.fields(cls) if f.name not in ("scope",)]


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class SummaryStatistic(StructureBase):
    """Description of a single summary static under a Scope."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/SummaryStatistic"

    @yaml_object
    class Combine(Enumeration, yaml_tag="!meta.db/v4/SummaryStatistic.Combine"):
        # Added in v4.0
        sum = (0, 0)
        min = (1, 0)
        max = (2, 0)

    scope: "PropagationScope"
    formula: str
    combine: Combine
    statMetricId: int

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
        return (
            f"{self.combine.name} / '{self.formula}' / {self.scope.scopeName}  #{self.statMetricId}"
        )

    @classmethod
    def from_file(
        cls, version: int, file, offset: int, scopesByOffset: dict[int, "PropagationScope"]
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scope=scopesByOffset[data["pScope"]],
            formula=read_ntstring(file, data["pFormula"]),
            combine=cls.Combine.versioned_decode(version, data["combine"]),
            statMetricId=data["statMetricId"],
        )

    @classmethod
    def owning_fields(cls) -> list[dataclasses.Field]:
        return [f for f in dataclasses.fields(cls) if f.name not in ("scope",)]


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class PropagationScope(StructureBase):
    """Description of a single propagation scope."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/PropagationScope"

    @yaml_object
    class Type(Enumeration, yaml_tag="!meta.db/v4/PropagationScope.Type"):
        # Added in v4.0
        custom = (0, 0)
        point = (1, 0)
        execution = (2, 0)
        transitive = (3, 0)

    scopeName: str
    type: Type
    propagationIndex: int

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
            suffix = f" bit/{self.propagationIndex}"
        return f"{self.scopeName} [{self.type.name}]" + suffix

    @classmethod
    def from_file(cls, version: int, file, offset: int):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            scopeName=read_ntstring(file, data["pScopeName"]),
            type=cls.Type.versioned_decode(version, data["type"]),
            propagationIndex=data["propagationIndex"],
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class LoadModules(StructureBase):
    """meta.db Load Modules section."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/LoadModules"

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
        modulesByOffset = {
            o: Module.from_file(version, file, o)
            for o in scaled_range(data["pModules"], data["nModules"], data["szModule"])
        }
        return (
            cls(
                modules=list(modulesByOffset.values()),
            ),
            modulesByOffset,
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Module(StructureBase):
    """Description for a single load module."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/Module"

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
        result = self.path
        if "/" in result:
            result = result[result.rfind("/") :]
        return result

    @property
    def shorthand(self) -> str:
        return f"{self.shortpath} [{','.join(e.name for e in self.Flags if e in self.flags)}]"

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

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/SourceFiles"

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
        filesByOffset = {
            o: File.from_file(version, file, o)
            for o in scaled_range(data["pFiles"], data["nFiles"], data["szFile"])
        }
        return cls(files=list(filesByOffset.values())), filesByOffset


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class File(StructureBase):
    """Description for a single source file."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/File"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!meta.db/v4/File.Flags"):
        # Added in v4.0
        copied = (0, 0)

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
        result = self.path
        if "/" in result:
            result = result[result.rfind("/") :]
        return result

    @property
    def shorthand(self) -> str:
        return f"{self.shortpath} [{','.join(e.name for e in self.Flags if e in self.flags)}]"

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

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/Functions"

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
        modulesByOffset: dict[int, Module],
        filesByOffset: dict[int, File],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        functionsByOffset = {
            o: Function.from_file(
                version, file, o, modulesByOffset=modulesByOffset, filesByOffset=filesByOffset
            )
            for o in scaled_range(data["pFunctions"], data["nFunctions"], data["szFunction"])
        }
        return cls(functions=list(functionsByOffset.values())), functionsByOffset


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Function(StructureBase):
    """Description for a single performance metric."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/Function"

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
        return f"{self.name}{srcloc}{point} [{','.join(e.name for e in self.Flags if e in self.flags)}]"

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modulesByOffset: dict[int, Module],
        filesByOffset: dict[int, File],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            name=read_ntstring(file, data["pName"]),
            module=modulesByOffset[data["pModule"]] if data["pModule"] != 0 else None,
            offset=data["offset"],
            file=filesByOffset[data["pFile"]] if data["pFile"] != 0 else None,
            line=data["line"],
            flags=cls.Flags.versioned_decode(version, data["flags"]),
        )

    @classmethod
    def owning_fields(cls) -> list[dataclasses.Field]:
        return [f for f in dataclasses.fields(cls) if f.name not in ("module", "file")]


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class ContextTree(StructureBase):
    """meta.db Context Tree section."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/ContextTree"

    entryPoints: list["EntryPoint"]

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
        modulesByOffset: dict[int, Module],
        filesByOffset: dict[int, File],
        functionsByOffset: dict[int, Function],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            entryPoints=[
                EntryPoint.from_file(
                    version,
                    file,
                    o,
                    modulesByOffset=modulesByOffset,
                    filesByOffset=filesByOffset,
                    functionsByOffset=functionsByOffset,
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

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/EntryPoint"

    @yaml_object
    class EntryPoint(Enumeration, yaml_tag="!meta.db/v4/EntryPoint.EntryPoint"):
        # Added in v4.0
        unknown_entry = (0, 0)
        main_thread = (1, 0)
        application_thread = (2, 0)

    ctxId: int
    entryPoint: EntryPoint
    prettyName: str
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
        return f"{self.prettyName} (= {self.entryPoint.name})  #{self.ctxId}"

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modulesByOffset: dict[int, Module],
        filesByOffset: dict[int, File],
        functionsByOffset: dict[int, Function],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        return cls(
            children=Context.multi_from_file(
                version,
                file,
                data["pChildren"],
                data["szChildren"],
                modulesByOffset=modulesByOffset,
                filesByOffset=filesByOffset,
                functionsByOffset=functionsByOffset,
            ),
            ctxId=data["ctxId"],
            entryPoint=cls.EntryPoint.versioned_decode(version, data["entryPoint"]),
            prettyName=read_ntstring(file, data["pPrettyName"]),
        )


@yaml_object
@dataclasses.dataclass(eq=False, kw_only=True)
class Context(StructureBase):
    """Description for a calling (or otherwise) context."""

    yaml_tag: T.ClassVar[str] = "!meta.db/v4/Context"

    @yaml_object
    class Flags(BitFlags, yaml_tag="!meta.db/v4/Context.Flags"):
        # Added in v4.0
        hasFunction = (0, 0)
        hasSrcLoc = (1, 0)
        hasPoint = (2, 0)

    @yaml_object
    class Relation(Enumeration, yaml_tag="!meta.db/v4/Context.Relation"):
        # Added in v4.0
        lexical = (0, 0)
        call = (1, 0)
        inlined_call = (2, 0)

    @yaml_object
    class LexicalType(Enumeration, yaml_tag="!meta.db/v4/Context.LexicalType"):
        # Added in v4.0
        function = (0, 0)
        loop = (1, 0)
        line = (2, 0)
        instruction = (3, 0)

    ctxId: int
    flags: Flags
    relation: Relation
    lexicalType: LexicalType
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
        if self.Flags.hasFunction in self.flags:
            if self.function is None:
                raise ValueError("hasFunction implies function is not None!")
        elif self.function is not None:
            raise ValueError("!hasFunction implies function is None!")

        if self.Flags.hasSrcLoc in self.flags:
            if self.file is None or self.line is None:
                raise ValueError("hasSrcLoc implies file and line are not None!")
        elif self.file is not None or self.line is not None:
            raise ValueError("!hasSrcLoc implies file and line are None!")

        if self.Flags.hasPoint in self.flags:
            if self.module is None or self.offset is None:
                raise ValueError("hasPoint implies module and offset are not None!")
        elif self.module is not None or self.offset is not None:
            raise ValueError("!hasPoint implies module and offset are None!")

    @property
    def shorthand(self) -> str:
        bits = []
        if self.Flags.hasFunction in self.flags:
            bits.append(self.function.name)
        if self.Flags.hasSrcLoc in self.flags:
            bits.append(f"{self.file.shortpath}:{self.line:d}")
        if self.Flags.hasPoint in self.flags:
            bits.append(f"{self.module.shortpath}+0x{self.offset:x}")
        return f"-{self.relation.name}> [{self.lexicalType.name}] {' '.join(bits)}  #{self.ctxId}"

    @classmethod
    def from_file(
        cls,
        version: int,
        file,
        offset: int,
        modulesByOffset: dict[int, Module],
        filesByOffset: dict[int, File],
        functionsByOffset: dict[int, Function],
    ):
        data = cls.__struct.unpack_file(version, file, offset)
        data["flags"] = cls.Flags.versioned_decode(version, data["flags"])

        words = []

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
        if cls.Flags.hasFunction in data["flags"]:
            data["function"] = functionsByOffset[read_subfield("<Q")]

        data["file"] = data["line"] = None
        if cls.Flags.hasSrcLoc in data["flags"]:
            data["file"] = filesByOffset[read_subfield("<Q")]
            data["line"] = read_subfield("<L")

        data["module"] = data["offset"] = None
        if cls.Flags.hasPoint in data["flags"]:
            data["module"] = modulesByOffset[read_subfield("<Q")]
            data["offset"] = read_subfield("<Q")

        return (
            cls(
                children=cls.multi_from_file(
                    version,
                    file,
                    data["pChildren"],
                    data["szChildren"],
                    modulesByOffset=modulesByOffset,
                    filesByOffset=filesByOffset,
                    functionsByOffset=functionsByOffset,
                ),
                ctxId=data["ctxId"],
                flags=data["flags"],
                relation=cls.Relation.versioned_decode(version, data["relation"]),
                lexicalType=cls.LexicalType.versioned_decode(version, data["lexicalType"]),
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
    def owning_fields(cls) -> list[dataclasses.Field]:
        return [f for f in dataclasses.fields(cls) if f.name not in ("function", "module", "file")]
