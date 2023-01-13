import abc
import collections.abc
import dataclasses
import functools
import re
import typing as T

from hpctoolkit.formats.v4 import metadb

"""
Utilities to help match parts of Context sub-trees.
"""


class _MatchAny:
    """Matches any value passed to it"""

    def matches(self, _got: T.Any) -> T.Literal[True]:
        return True


_T = T.TypeVar("T")
_C = T.TypeVar("C")
_V = T.TypeVar("V")


def _coercion(
    func: collections.abc.Callable[[_T, _V], _C]
) -> collections.abc.Callable[[_T, _V | _MatchAny], _C | _MatchAny]:
    @classmethod
    @functools.wraps(func)
    def wrapper(cls: _T, val: _V | _MatchAny) -> _C | _MatchAny:
        if isinstance(val, _MatchAny):
            return val
        return func(cls, val)

    return wrapper


class _MatchExact:
    """Matches an exact value"""

    def __init__(self, value: T.Any):
        self._value = value

    def matches(self, got: T.Any) -> bool:
        return got == self._value

    @_coercion
    def coerce(cls, val: T.Any) -> "_MatchExact":
        return _MatchExact(val)


def _coercion_to_exact(exact: T.Type):
    def apply(
        func: collections.abc.Callable[[_T, _V], _C]
    ) -> collections.abc.Callable[[_T, _V | exact | _MatchAny], _C | _MatchExact | _MatchAny]:
        @classmethod
        @functools.wraps(func)
        def wrapper(cls: _T, val: _V | exact | _MatchAny) -> _C | _MatchExact | _MatchAny:
            if isinstance(val, _MatchAny):
                return val
            if isinstance(val, exact):
                return _MatchExact(val)
            return func(cls, val)

        return wrapper

    return apply


class _MatchStrBase(abc.ABC):
    """Match a single string"""

    @abc.abstractmethod
    def matches(self, got: str) -> bool:
        """Test if the given string satisfies the requirements"""

    @_coercion
    def coerce(cls, expected: str) -> "_MatchStrBase":
        return cls(expected)


class MatchStr(_MatchStrBase):
    """Match a single string completely"""

    def __init__(self, expected: str):
        self.expected = expected

    def matches(self, got: str) -> bool:
        return got == self.expected


class MatchStrEndsWith(_MatchStrBase):
    """Match if a string ends with the given string"""

    def __init__(self, expectedend: str):
        self.expectedend = expectedend

    def matches(self, got: str) -> bool:
        return got.endswith(self.expectedend)


class MatchFile:
    """Match a single File with the specified attributes set"""

    def __init__(self, *, path=_MatchAny()):
        self._submatch = dict(
            path=MatchStrEndsWith.coerce(path),
        )

    def matches(self, f: metadb.File) -> bool:
        """Test if the given File satisfies the requirements"""
        return all(v.matches(getattr(f, k)) for k, v in self._submatch.items())

    def itermatch(self, fs: metadb.SourceFiles) -> collections.abc.Iterator[metadb.File]:
        """Iterate over all matching Files"""
        return filter(self.matches, fs.files)

    @_coercion_to_exact(metadb.File | None)
    def coerce(cls, val) -> "MatchFile":
        return cls(path=val)


class MatchModule:
    """Match a single Module with the specified attributes set"""

    def __init__(self, *, path=_MatchAny()):
        self._submatch = dict(
            path=MatchStrEndsWith.coerce(path),
        )

    def matches(self, m: metadb.Module) -> bool:
        """Test if the given Module satisfies the requirements"""
        return all(v.matches(getattr(m, k)) for k, v in self._submatch.items())

    def itermatch(self, lm: metadb.LoadModules) -> collections.abc.Iterator[metadb.Module]:
        """Iterate over all matching Modules"""
        return filter(self.matches, lm.modules)

    @_coercion_to_exact(metadb.Module | None)
    def coerce(cls, val) -> "MatchModule":
        return cls(path=val)


class MatchFunction:
    """Match a single Function with the specified attributes set"""

    def __init__(
        self,
        *,
        name=_MatchAny(),
        module=_MatchAny(),
        offset: int | _MatchAny = _MatchAny(),
        file=_MatchAny(),
        line: int | _MatchAny = _MatchAny(),
    ):
        self._submatch = dict(
            name=MatchStr.coerce(name),
            module=MatchModule.coerce(module),
            offset=_MatchExact.coerce(offset),
            file=MatchFile.coerce(file),
            line=_MatchExact.coerce(line),
        )

    def matches(self, f: metadb.Function) -> bool:
        """Test if the given Function satisfies the requirements"""
        return all(v.matches(getattr(f, k)) for k, v in self._submatch.items())

    def itermatch(self, fs: metadb.Functions) -> collections.abc.Iterator[metadb.Function]:
        """Iterate over all matching Functions"""
        return filter(self.matches, fs.functions)

    @_coercion_to_exact(metadb.Function | None)
    def coerce(cls, val) -> "MatchFunction":
        return cls(name=val)


class MatchEntryPoint:
    """Match any EntryPoint with the specified attributes set"""

    @_coercion_to_exact(metadb.EntryPoint.EntryPoint)
    def _coerce_entryPoint(cls, val) -> _MatchExact:
        return _MatchExact(metadb.EntryPoint.EntryPoint[val])

    def __init__(self, *, entryPoint=_MatchAny(), prettyName=_MatchAny()):
        self._submatch = dict(
            entryPoint=self._coerce_entryPoint(entryPoint),
            prettyName=MatchStr.coerce(prettyName),
        )

    def matches(self, ep: metadb.EntryPoint) -> bool:
        """Test if the given EntryPoint satisfies this matcher's requirements"""
        return all(v.matches(getattr(ep, k)) for k, v in self._submatch.items())

    def itermatch(self, ct: metadb.ContextTree) -> collections.abc.Iterator[metadb.EntryPoint]:
        """Iterate over all EntryPoints that satisfy this matcher's requirements"""
        return filter(self.matches, ct.entryPoints)

    @_coercion_to_exact(metadb.EntryPoint)
    def coerce(cls, val) -> "MatchEntryPoint":
        return cls(entryPoint=val)


class MatchCtx:
    """Match a single Context with the specified attributes set"""

    @_coercion_to_exact(metadb.Context.Relation)
    def _coerce_relation(cls, val) -> _MatchExact:
        return _MatchExact(metadb.Context.Relation[val])

    @_coercion_to_exact(metadb.Context.LexicalType)
    def _coerce_lexicalType(cls, val) -> _MatchExact:
        return _MatchExact(metadb.Context.LexicalType[val])

    def __init__(
        self,
        *,
        relation=_MatchAny(),
        lexicalType=_MatchAny(),
        function=_MatchAny(),
        file=_MatchAny(),
        line: int | _MatchAny = _MatchAny(),
        module=_MatchAny(),
        offset: int | _MatchAny = _MatchAny(),
    ):
        self._submatch = dict(
            relation=self._coerce_relation(relation),
            lexicalType=self._coerce_lexicalType(lexicalType),
            function=MatchFunction.coerce(function),
            file=MatchFile.coerce(file),
            line=_MatchExact.coerce(line),
            module=MatchModule.coerce(module),
            offset=_MatchExact.coerce(offset),
        )

    def matches(self, c: metadb.Context) -> bool:
        """Test if the given Context satisfies the requirements"""
        return all(v.matches(getattr(c, k)) for k, v in self._submatch.items())

    def itermatch(
        self, c: metadb.Context | metadb.EntryPoint
    ) -> collections.abc.Iterator[metadb.Context]:
        """Iterate over all child Contexts that satisfy the requirements"""
        return filter(self.matches, c.children)


def chainmatch(val, matcher, *later_matches) -> collections.abc.Iterator:
    """Chain together a number of matchers."""
    for got in matcher.itermatch(val):
        if len(later_matches) > 0:
            yield from chainmatch(got, *later_matches)
        else:
            yield got
