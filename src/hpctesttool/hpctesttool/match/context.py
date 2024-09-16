# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import abc
import collections.abc
import functools
import typing

from hpctesttool.formats.v4 import metadb


class _MatchAny:
    """Matches any value passed to it."""

    def matches(self, _got: typing.Any) -> typing.Literal[True]:
        return True

    def __str__(self) -> str:
        return "ANY"


_MatchAnySingleton = _MatchAny()


def _coercion(func):
    @functools.wraps(func)
    def wrapper(cls, val):
        if isinstance(val, (_MatchAny, cls)):
            return val
        return func(cls, val)

    return wrapper


class _MatchExact:
    """Matches an exact value."""

    def __init__(self, value: typing.Any):
        self._value = value

    def matches(self, got: typing.Any) -> bool:
        return got == self._value

    def __str__(self) -> str:
        return repr(self._value)

    @classmethod
    @_coercion
    def coerce(cls, val: typing.Any) -> "_MatchExact":
        return _MatchExact(val)


def _coercion_to_exact(exact: typing.Type, *, coerce_none: bool = False):
    def apply(func):
        @functools.wraps(func)
        def wrapper(cls, val):
            if isinstance(val, (_MatchAny, cls)):
                return val
            if isinstance(val, exact) or (coerce_none and val is None):
                return _MatchExact(val)
            return func(cls, val)

        return wrapper

    return apply


class _MatchStrBase(abc.ABC):
    """Match a single string."""

    @abc.abstractmethod
    def __init__(self, expected: str):
        pass

    @abc.abstractmethod
    def matches(self, got: str) -> bool:
        """Test if the given string satisfies the requirements."""

    @classmethod
    @_coercion
    def coerce(cls, expected: str) -> "_MatchStrBase":
        return cls(expected)


class MatchStr(_MatchStrBase):
    """Match a single string completely."""

    def __init__(self, expected: str):
        self.expected = expected

    def matches(self, got: str) -> bool:
        return got == self.expected

    def __str__(self) -> str:
        return repr(self.expected)


class MatchStrEndsWith(_MatchStrBase):
    """Match if a string ends with the given string."""

    def __init__(self, expectedend: str):
        self.expectedend = expectedend

    def matches(self, got: str) -> bool:
        return got.endswith(self.expectedend)

    def __str__(self) -> str:
        return "* + " + repr(self.expectedend)


class MatchFile:
    """Match a single File with the specified attributes set."""

    def __init__(self, *, path=_MatchAnySingleton):
        self._submatch = {
            "path": MatchStrEndsWith.coerce(path),
        }

    def matches(self, f: metadb.File) -> bool:
        """Test if the given File satisfies the requirements."""
        return all(v.matches(getattr(f, k)) for k, v in self._submatch.items())

    def __str__(self) -> str:
        return (
            "File("
            + ", ".join(
                [
                    f"{k}={m}"
                    for k, m in self._submatch.items()
                    if not isinstance(m, _MatchAny)
                ]
            )
            + ")"
        )

    def itermatch(
        self, fs: metadb.SourceFiles
    ) -> "collections.abc.Iterator[metadb.File]":
        """Iterate over all matching Files."""
        return filter(self.matches, fs.files)

    @classmethod
    @_coercion_to_exact(metadb.File, coerce_none=True)
    def coerce(cls, val) -> "MatchFile":
        return cls(path=val)


class MatchModule:
    """Match a single Module with the specified attributes set."""

    def __init__(self, *, path=_MatchAnySingleton):
        self._submatch = {
            "path": MatchStrEndsWith.coerce(path),
        }

    def matches(self, m: metadb.Module) -> bool:
        """Test if the given Module satisfies the requirements."""
        return all(v.matches(getattr(m, k)) for k, v in self._submatch.items())

    def __str__(self) -> str:
        return (
            "Module("
            + ", ".join(
                [
                    f"{k}={m}"
                    for k, m in self._submatch.items()
                    if not isinstance(m, _MatchAny)
                ]
            )
            + ")"
        )

    def itermatch(
        self, lm: metadb.LoadModules
    ) -> "collections.abc.Iterator[metadb.Module]":
        """Iterate over all matching Modules."""
        return filter(self.matches, lm.modules)

    @classmethod
    @_coercion_to_exact(metadb.Module, coerce_none=True)
    def coerce(cls, val) -> "MatchModule":
        return cls(path=val)


class MatchFunction:
    """Match a single Function with the specified attributes set."""

    def __init__(
        self,
        *,
        name=_MatchAnySingleton,
        module=_MatchAnySingleton,
        offset: typing.Union[int, _MatchAny] = _MatchAnySingleton,
        file=_MatchAnySingleton,
        line: typing.Union[int, _MatchAny] = _MatchAnySingleton,
    ):
        self._submatch = {
            "name": MatchStr.coerce(name),
            "module": MatchModule.coerce(module),
            "offset": _MatchExact.coerce(offset),
            "file": MatchFile.coerce(file),
            "line": _MatchExact.coerce(line),
        }

    def matches(self, f: metadb.Function) -> bool:
        """Test if the given Function satisfies the requirements."""
        return all(v.matches(getattr(f, k)) for k, v in self._submatch.items())

    def __str__(self) -> str:
        return (
            "Function("
            + ", ".join(
                [
                    f"{k}={m}"
                    for k, m in self._submatch.items()
                    if not isinstance(m, _MatchAny)
                ]
            )
            + ")"
        )

    def itermatch(
        self, fs: metadb.Functions
    ) -> "collections.abc.Iterator[metadb.Function]":
        """Iterate over all matching Functions."""
        return filter(self.matches, fs.functions)

    @classmethod
    @_coercion_to_exact(metadb.Function, coerce_none=True)
    def coerce(cls, val) -> "MatchFunction":
        return cls(name=val)


class MatchEntryPoint:
    """Match any EntryPoint with the specified attributes set."""

    @classmethod
    @_coercion_to_exact(metadb.EntryPoint.EntryPoint)
    def _coerce_entry_point(cls, val) -> _MatchExact:
        return _MatchExact(metadb.EntryPoint.EntryPoint[val])

    def __init__(
        self, *, entry_point=_MatchAnySingleton, pretty_name=_MatchAnySingleton
    ):
        self._submatch = {
            "entry_point": self._coerce_entry_point(entry_point),
            "pretty_name": MatchStr.coerce(pretty_name),
        }

    def matches(self, ep: metadb.EntryPoint) -> bool:
        """Test if the given EntryPoint satisfies this matcher's requirements."""
        return all(v.matches(getattr(ep, k)) for k, v in self._submatch.items())

    def __str__(self) -> str:
        return (
            "EntryPoint("
            + ", ".join(
                [
                    f"{k}={m}"
                    for k, m in self._submatch.items()
                    if not isinstance(m, _MatchAny)
                ]
            )
            + ")"
        )

    def itermatch(
        self, ct: metadb.ContextTree
    ) -> "collections.abc.Iterator[metadb.EntryPoint]":
        """Iterate over all EntryPoints that satisfy this matcher's requirements."""
        return filter(self.matches, ct.entry_points)

    @classmethod
    @_coercion_to_exact(metadb.EntryPoint)
    def coerce(cls, val) -> "MatchEntryPoint":
        return cls(entry_point=val)


class MatchCtx:
    """Match a single Context with the specified attributes set."""

    @classmethod
    @_coercion_to_exact(metadb.Context.Relation)
    def _coerce_relation(cls, val) -> _MatchExact:
        return _MatchExact(metadb.Context.Relation[val])

    @classmethod
    @_coercion_to_exact(metadb.Context.LexicalType)
    def _coerce_lexical_type(cls, val) -> _MatchExact:
        return _MatchExact(metadb.Context.LexicalType[val])

    def __init__(
        self,
        *,
        relation=_MatchAnySingleton,
        lexical_type=_MatchAnySingleton,
        function=_MatchAnySingleton,
        file=_MatchAnySingleton,
        line: typing.Union[int, _MatchAny] = _MatchAnySingleton,
        module=_MatchAnySingleton,
        offset: typing.Union[int, _MatchAny] = _MatchAnySingleton,
    ):
        self._submatch = {
            "relation": self._coerce_relation(relation),
            "lexical_type": self._coerce_lexical_type(lexical_type),
            "function": MatchFunction.coerce(function),
            "file": MatchFile.coerce(file),
            "line": _MatchExact.coerce(line),
            "module": MatchModule.coerce(module),
            "offset": _MatchExact.coerce(offset),
        }

    def matches(self, c: metadb.Context) -> bool:
        """Test if the given Context satisfies the requirements."""
        return all(v.matches(getattr(c, k)) for k, v in self._submatch.items())

    def __str__(self) -> str:
        return (
            "Context("
            + ", ".join(
                [
                    f"{k}={m}"
                    for k, m in self._submatch.items()
                    if not isinstance(m, _MatchAny)
                ]
            )
            + ")"
        )

    def itermatch(
        self, c: typing.Union[metadb.Context, metadb.EntryPoint]
    ) -> "collections.abc.Iterator[metadb.Context]":
        """Iterate over all child Contexts that satisfy the requirements."""
        return filter(self.matches, c.children)


def chainmatch(val, matcher, *later_matches) -> collections.abc.Iterator:
    """Chain together a number of matchers."""
    for got in matcher.itermatch(val):
        if len(later_matches) > 0:
            yield from chainmatch(got, *later_matches)
        else:
            yield got
