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

import abc
import dataclasses
import enum
import functools
import io
import struct
import typing as T
import warnings
from pathlib import Path

import ruamel.yaml

from ._util import VersionedStructure, preserve_filepos

__all__ = [
    "yaml_object",
    "canonical_paths",
    "StructureBase",
    "DatabaseBase",
    "DatabaseFile",
    "Enumeration",
    "BitFlags",
    "InvalidFormatError",
    "IncompatibleFormatError",
    "UnsupportedFormatWarning",
]


yamls = [ruamel.yaml.YAML(typ=x) for x in ("rt", "safe", "base", "unsafe", "rtsc")]


def yaml_object(cls):
    for y in yamls:
        y.register_class(cls)
    return cls


@yaml_object
class _CommentedMap(ruamel.yaml.comments.CommentedMap):
    yaml_tag = "tag:yaml.org,2002:map"

    @classmethod
    def to_yaml(cls, representer, obj):
        return representer.represent_mapping(cls.yaml_tag, obj)

    @classmethod
    def from_yaml(cls, constructor, node):
        return constructor.construct_mapping(node)


@yaml_object
class _CommentedSeq(ruamel.yaml.comments.CommentedSeq):
    yaml_tag = "tag:yaml.org,2002:seq"

    @classmethod
    def to_yaml(cls, representer, obj):
        return representer.represent_sequence(cls.yaml_tag, obj)

    @classmethod
    def from_yaml(cls, constructor, node):
        return constructor.construct_sequence(node)


class StructureBase(abc.ABC):
    """Base class for all structures within the formats. These are the main "nodes" in the
    object-graph that every database represents."""

    @property
    def shorthand(self) -> str | None:
        """Return a short 1-line string that can be used to describe this object, or None if the
        class name is a sufficient shorthand for this object."""
        return None

    def __str__(self) -> str:
        sh = self.shorthand
        return super().__str__() if sh is None else f"{self.__class__.__name__}({sh})"

    @classmethod
    def owning_fields(cls) -> list[dataclasses.Field]:
        """List the fields that objects of this type "own". Recursing only through "owned" fields
        will result in a perfect tree iteration rather than a DAG iteration."""
        return dataclasses.fields(cls)

    def __getstate__(self) -> dict[str, T.Any]:
        return {f.name: getattr(self, f.name) for f in dataclasses.fields(self)}

    @classmethod
    def _as_commented_map(cls, obj):
        if hasattr(obj, "__getstate__"):
            state = obj.__getstate__()
        else:
            state = obj.__dict__.copy()
        assert isinstance(state, dict)
        rstate = ruamel.yaml.comments.CommentedMap(state)
        for k, v in rstate.items():
            if isinstance(v, StructureBase):
                if v.shorthand is not None:
                    rstate.yaml_add_eol_comment(v.shorthand, key=k)
            elif isinstance(v, list) and any(isinstance(vv, StructureBase) for vv in v):
                rstate[k] = _CommentedSeq(v)
                for i, vv in enumerate(rstate[k]):
                    if isinstance(vv, StructureBase):
                        if vv.shorthand is not None:
                            rstate[k].yaml_add_eol_comment(vv.shorthand, key=i)
        return rstate

    @classmethod
    def to_yaml(cls, representer, obj):
        return representer.represent_mapping(cls.yaml_tag, cls._as_commented_map(obj))


def canonical_paths(obj: StructureBase) -> dict[StructureBase, tuple[str | int]]:
    """Starting from the given Database* object, identify all objects within that object-graph and
    return a mapping from these objects to the attribute/index path required to access them."""
    result: dict[StructureBase, tuple[str | int]] = {}

    def add(o: StructureBase | list | dict, path: list[str | int]):
        if isinstance(o, StructureBase):
            result[o] = tuple(path)
            for f in o.owning_fields():
                path.append(f.name)
                add(getattr(o, f.name), path)
                del path[-1]
        elif isinstance(o, list):
            for i, v in enumerate(o):
                path.append(i)
                add(v, path)
                del path[-1]
        elif isinstance(o, dict):
            for k, v in o.items():
                assert isinstance(k, str | int)
                path.append(k)
                add(v, path)
                del path[-1]

    add(obj, [])
    return result


def get_from_path(obj: StructureBase, path: tuple[str | int]) -> StructureBase:
    """Access the object referenced by the given canonical path, which was generated by a prior call
    to canonical_paths. The obj may differ but should be the same type."""
    cursor: StructureBase | list | dict = obj
    for elem in path:
        if isinstance(cursor, StructureBase):
            cursor = getattr(cursor, elem)
        elif isinstance(cursor, list | dict):
            cursor = cursor[elem]
        else:
            assert False
    return cursor


class DatabaseFile(StructureBase):
    """Base class for the top-level object representing an entire *.db file."""

    @classmethod
    @abc.abstractmethod
    def from_file(cls, file):
        """Deserialize the entire file into objects."""

    __hdr_struct = struct.Struct("10s4sBB")

    major_version: T.ClassVar[int]
    max_minor_version: T.ClassVar[int]
    format_code: T.ClassVar[bytes]
    footer_code: T.ClassVar[bytes]
    yaml_tag: T.ClassVar[str]

    @classmethod
    def _parse_header(cls, file) -> int:
        """Parse the header (and footer) of the given file. Returns the minor version number.
        Raises standard errors and warnings for common format-incompatibility cases."""
        assert len(cls.format_code) == 4
        assert len(cls.footer_code) == 8
        fmtid, footer, major, minor = (
            cls.format_code,
            cls.footer_code,
            cls.major_version,
            cls.max_minor_version,
        )

        with preserve_filepos(file):
            file.seek(0)
            hdr_bits = file.read(16)
            if len(hdr_bits) < 16:
                raise OSError("Unable to read file header, file appears to be truncated")

            file.seek(-8, io.SEEK_END)
            in_footer = file.read(8)
            if len(in_footer) < 8:
                raise OSError("Unable to read file footer, file appears to be truncated")
        in_magic, in_fmtid, in_major, in_minor = cls.__hdr_struct.unpack(hdr_bits)

        if in_magic != b"HPCTOOLKIT":
            raise InvalidFormatError("File is not an HPCToolkit data file")
        if in_fmtid != fmtid:
            raise InvalidFormatError(
                f"File is of a different format: '{in_fmtid.decode('utf-8')}' != '{fmtid.decode('utf-8')}'"
            )
        if in_footer != footer:
            raise InvalidFormatError(
                f"File is of a different format: '{in_footer.decode('utf-8')}' != '{footer.decode('utf-8')}'"
            )
        if in_major != major:
            raise IncompatibleFormatError(
                f"This implementation is only able to read v{major:d}.X, got v{in_major:d}"
            )
        if in_minor > minor:
            warnings.warn(
                UnsupportedFormatWarning(
                    f"This implementation is only able to read version <={major:d}.{minor:d}, got v{in_major:d}.{in_minor:d}. Some data may be skipped."
                )
            )
        return in_minor

    @classmethod
    def _header_struct(cls, **sections: dict[str, tuple[int]]) -> VersionedStructure:
        """Generate a VersionedStructure for the section table of a FileHeader. The result will
        contain two fields per section, a 'p' prefix for the starting offset and a 'sz' prefix for
        the total size. For convenience the fields are already offset to skip over the initial
        file identifiers, so start from offset 0 in the file."""

        fields = {}
        offset = 16
        for name, (min_ver,) in sections.items():
            fields["sz" + name] = (min_ver, offset, "Q")
            fields["p" + name] = (min_ver, offset + 8, "Q")
            offset += 16
        return VersionedStructure("<", **fields)


class DatabaseBase(StructureBase):
    """Base class for the top-level object representing an entire performance database."""

    major_version: T.ClassVar[int]

    @classmethod
    @abc.abstractmethod
    def from_dir(cls, dbdir: Path):
        """Deserialize the entire database directory into appropriate DatabaseFile objects."""


EnumerationT = T.TypeVar("EnumerationT", bound="Enumeration")


class Enumeration(enum.Enum):
    """Base class for all enumerations in the formats."""

    def __new__(cls, value: int, min_version: int):
        obj = object.__new__(cls)
        obj._value_ = value
        obj.min_version = min_version
        return obj

    @classmethod
    def __init_subclass__(cls, /, yaml_tag: str, **kwargs):
        super().__init_subclass__(**kwargs)
        cls.yaml_tag = yaml_tag

    @classmethod
    def versioned_decode(
        cls: T.Type[EnumerationT], version: int, value: int
    ) -> EnumerationT | None:
        val = cls(value)  # noqa
        return val if val.min_version <= version else None

    @classmethod
    def to_yaml(cls, representer, node):
        representer.alias_key = None
        return representer.represent_scalar(cls.yaml_tag, node.name)

    @classmethod
    def from_yaml(cls, constructor, node):
        return cls[constructor.construct_scalar(node)]


BitFlagsT = T.TypeVar("BitFlagsT", bound="BitFlags")


class BitFlags(enum.Flag):
    """Base class for all flag-bits fields in the formats."""

    def __new__(cls, value: int, min_version: int | None = None):
        obj = object.__new__(cls)
        if min_version is not None:
            obj._value_ = 1 << value
            obj.min_version = min_version
        else:
            obj._value_ = value
        return obj

    @classmethod
    def __init_subclass__(cls, /, yaml_tag: str, **kwargs):
        super().__init_subclass__(**kwargs)
        cls.yaml_tag = yaml_tag

    @classmethod
    def versioned_decode(cls: T.Type[BitFlagsT], version: int, value: int) -> BitFlagsT:
        bits = [bit for bit in cls if bit.min_version <= version]
        return cls(value) & functools.reduce(lambda x, y: x | y, bits) if bits else cls(0)

    @classmethod
    def to_yaml(cls, representer, node):
        representer.alias_key = None
        return representer.represent_sequence(
            cls.yaml_tag, sorted(val.name for val in cls if val in node), flow_style=True
        )

    @classmethod
    def from_yaml(cls, constructor, node):
        bits = [cls[val] for val in constructor.construct_sequence(node, deep=True)]
        return functools.reduce(lambda x, y: x | y, bits) if bits else cls(0)


class InvalidFormatError(Exception):
    """Error raised when the input serialized data cannot be read due to structural errors, such as
    corruption."""


class IncompatibleFormatError(InvalidFormatError):
    """Error raised when the input serialized data is of an incompatible version compared to the
    library implementation."""


class UnsupportedFormatWarning(UserWarning):
    """Warning raised when the input serialized data is a compatible but newer version compared to
    the library implementation. Some data in the input may not be converted."""
