import atexit
import collections
import collections.abc
import contextlib
import dataclasses
import functools
import itertools
import os
import re
import shutil
import stat
import tempfile
import typing
from pathlib import Path

from ..meson import MesonMachineFile


class UnsatisfiableSpecError(Exception):
    """Exception raised when the given variant is unsatisfiable."""

    def __init__(self, missing):
        super().__init__(f"missing definition for argument {missing}")
        self.missing = missing


class ImpossibleSpecError(Exception):
    """Exception raised when the given variant is impossible."""

    def __init__(self, a, b):
        super().__init__(f'conflict between "{a}" and "{b}"')
        self.a, self.b = a, b


class Compilers:
    """State needed to derive configure-time arguments, based on simple configuration files."""

    def __init__(self, cc: str | None = None):
        self.configs: list[tuple[Path | None, str]] = []
        self._compiler: tuple[str, str] | None = None
        self._compiler_wrappers: tuple[Path, Path] | None = None
        if cc is not None:
            if not cc or cc[0] == "-":
                raise ValueError(cc)

            cc = cc.split("=")[0]  # Anything after an = is considered a comment
            mat = re.fullmatch("([^-]+)(-?.*)", cc)
            assert mat
            match mat.group(1):
                case "gcc":
                    self._compiler = "gcc" + mat.group(2), "g++" + mat.group(2)
                case "clang":
                    self._compiler = "clang" + mat.group(2), "clang++" + mat.group(2)
                case _:
                    raise ValueError(cc)

    @functools.cached_property
    def raw_compilers(self) -> tuple[str, str]:
        if self._compiler is not None:
            return self._compiler
        cc, cxx = os.environ.get("CC", "cc"), os.environ.get("CXX", "c++")
        if not shutil.which(cc) or not shutil.which(cxx):
            raise RuntimeError("Unable to guess system compilers, cc/c++ not present!")
        return cc, cxx

    @functools.cached_property
    def custom_compilers(self) -> tuple[str, str] | None:
        ccache = shutil.which("ccache")
        if self._compiler is not None or ccache:
            raw_cc, raw_cxx = self.raw_compilers
            if ccache:
                ccdir = Path(tempfile.mkdtemp(".ccwrap"))
                atexit.register(shutil.rmtree, ccdir, ignore_errors=True)
                cc, cxx = ccdir / "cc", ccdir / "cxx"
                with open(cc, "w", encoding="utf-8") as f:
                    f.write(f'#!/bin/sh\nexec {ccache} {raw_cc} "$@"')
                with open(cxx, "w", encoding="utf-8") as f:
                    f.write(f'#!/bin/sh\nexec {ccache} {raw_cxx} "$@"')
                cc.chmod(cc.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
                cxx.chmod(cxx.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
                return str(cc), str(cxx)
            return raw_cc, raw_cxx
        return None

    @functools.cached_property
    def compilers(self) -> tuple[str, str]:
        if self.custom_compilers is not None:
            return self.custom_compilers
        return self.raw_compilers


@dataclasses.dataclass(kw_only=True, frozen=True)
class ConcreteSpecification:
    """Point in the build configuration space, represented as a series of boolean-valued variants."""

    mpi: bool
    debug: bool
    valgrind_debug: bool
    papi: bool
    python: bool
    opencl: bool
    cuda: bool
    rocm: bool
    level0: bool
    gtpin: bool

    def __post_init__(self, **_kwargs):
        if self.gtpin and not self.level0:
            raise ImpossibleSpecError("+gtpin", "~level0")
        if self.valgrind_debug and not self.debug:
            raise ImpossibleSpecError("+valgrind_debug", "~debug")

    @classmethod
    def all_possible(cls) -> collections.abc.Iterable["ConcreteSpecification"]:
        """List all possible ConcreteSpecifications in the configuration space."""
        dims = [
            # (variant, values...), in order from slowest- to fastest-changing
            ("gtpin", False, True),
            ("level0", False, True),
            ("rocm", False, True),
            ("cuda", False, True),
            ("opencl", False, True),
            ("python", False, True),
            ("papi", True, False),
            ("valgrind_debug", False, True),
            ("debug", True, False),
            ("mpi", False, True),
        ]
        for point in itertools.product(*[[(x[0], val) for val in x[1:]] for x in dims]):
            with contextlib.suppress(ImpossibleSpecError):
                yield cls(**dict(point))

    @classmethod
    def variants(cls) -> tuple[str, ...]:
        """List the names of all the available variants."""
        return tuple(f.name for f in dataclasses.fields(cls))

    def __str__(self) -> str:
        return " ".join(f"+{n}" if getattr(self, n) else f"~{n}" for n in self.variants())

    @property
    def without_spaces(self) -> str:
        return "".join(f"+{n}" if getattr(self, n) else f"~{n}" for n in self.variants())


class Specification:
    """Subset of the build configuration space."""

    @dataclasses.dataclass
    class _Condition:
        min_enabled: int
        max_enabled: int

    _concrete_cls: typing.ClassVar[type[ConcreteSpecification]] = ConcreteSpecification
    _clauses: dict[tuple[str, ...], _Condition] | bool

    def _parse_long(self, valid_variants: frozenset[str], clause: str) -> set[str]:
        mat = re.fullmatch(r"\(([\w\s]+)\)\[([+~><=\d\s]+)\]", clause)
        if not mat:
            raise ValueError(f"Invalid clause: {clause!r}")

        variants = mat.group(1).split()
        if not variants:
            raise ValueError(f"Missing variants listing: {clause!r}")
        for v in variants:
            if v not in valid_variants:
                raise ValueError("Invalid variant: {v}")
        variants = tuple(sorted(variants))

        assert isinstance(self._clauses, dict)
        cond = self._clauses.setdefault(variants, self._Condition(0, len(variants)))
        for c in mat.group(2).split():
            cmat = re.fullmatch(r"([+~])([><=])(\d+)", c)
            if not cmat:
                raise ValueError(f"Invalid conditional expression: {c!r}")
            match cmat.group(1):
                case "+":
                    n, op = int(cmat.group(3)), cmat.group(2)
                case "~":
                    # ~>N is +<(V-N), etc.
                    n = len(variants) - int(cmat.group(3))
                    op = {">": "<", "<": ">", "=": "="}[cmat.group(2)]
                case _:
                    raise AssertionError()
            for base_op in {">": ">", "<": "<", "=": "<>"}[op]:
                match base_op:
                    case ">":
                        cond.min_enabled = max(cond.min_enabled, n)
                    case "<":
                        cond.max_enabled = min(cond.max_enabled, n)
                    case _:
                        raise AssertionError()
        return set(variants)

    def _parse_short(self, valid_variants: frozenset[str], clause: str) -> set[str]:
        mat = re.fullmatch(r"([+~])(\w+)", clause)
        if not mat:
            raise ValueError(f"Invalid clause: {clause!r}")

        match mat.group(1):
            case "+":
                cnt = 1
            case "~":
                cnt = 0
            case _:
                raise AssertionError()

        variant = mat.group(2)
        if variant not in valid_variants:
            raise ValueError(f"Invalid variant: {variant}")

        assert isinstance(self._clauses, dict)
        cond = self._clauses.setdefault((variant,), self._Condition(0, 1))
        cond.min_enabled = max(cond.min_enabled, cnt)
        cond.max_enabled = min(cond.max_enabled, cnt)
        return {variant}

    def _optimize(self):
        assert isinstance(self._clauses, dict)

        # Fold multi-variant clauses that have a single solution into single-variant clauses
        new_clauses: dict[tuple[str, ...], Specification._Condition] = {}
        for vrs, cond in self._clauses.items():
            assert len(vrs) > 0
            assert cond.min_enabled <= len(vrs)
            assert cond.max_enabled <= len(vrs)
            if (
                len(vrs) > 1
                and cond.min_enabled == cond.max_enabled
                and cond.min_enabled in (0, len(vrs))
            ):
                for v in vrs:
                    newcond = new_clauses.setdefault((v,), self._Condition(0, 1))
                    newcond.min_enabled = max(newcond.min_enabled, cond.min_enabled)
                    newcond.max_enabled = min(newcond.max_enabled, cond.max_enabled)
            else:
                new_clauses[vrs] = cond
        self._clauses = new_clauses

        # Prune clauses that trivially can't be satisfied or are always satisfied. For cleanliness.
        good_clauses = {}
        for vrs, cond in self._clauses.items():
            assert len(vrs) > 0
            assert cond.min_enabled <= len(vrs)
            assert cond.max_enabled <= len(vrs)
            if cond.min_enabled <= cond.max_enabled and (
                cond.min_enabled > 0 or cond.max_enabled < len(vrs)
            ):
                good_clauses[vrs] = cond
        self._clauses = good_clauses if good_clauses else False

    def __init__(  # noqa: C901
        self,
        spec: str,
        /,
        *,
        allow_blank: bool = False,
        allow_empty: bool = False,
        strict: bool = True,
    ):
        """Create a new Specification from the given specification string.

        The syntax follows this rough BNF grammar:
            spec := all | none | W { clause W }*
            clause := '!' variant
                      | flag variant
                      | '(' W variant { W variant }* W ')[' W condition { W condition }* W ']'
            flag := '+' | '~'
            condition := flag comparison N
            comparison := '>' | '<' | '='
            variant := # any variant listed in ConcreteSpecification.variants()
            N := # any base 10 number
            W := # any sequence of whitespace
        """
        valid_variants = frozenset(self._concrete_cls.variants())
        constrained_variants = set()

        # Parse the spec itself
        match spec.strip():
            case "all":
                self._clauses = True
            case "none":
                self._clauses = False
            case _:
                self._clauses = {}
                for token in re.split(r"(\([^)]+\)\[[^]]+\])|\s", spec):
                    if token is None or not token:
                        continue
                    if token[0] == "!":
                        if token[1:] not in valid_variants:
                            raise ValueError(f"Invalid variant: {token[1:]}")
                        constrained_variants.add(token[1:])
                    elif token[0] == "(":
                        constrained_variants |= self._parse_long(valid_variants, token)
                    else:
                        constrained_variants |= self._parse_short(valid_variants, token)
                if not self._clauses:
                    if allow_blank:
                        self._clauses = True
                    else:
                        raise ValueError("Blank specification not permitted")
                else:
                    self._optimize()

        # Check that any unconstrained variants actually do not vary
        if not isinstance(self._clauses, bool):
            bad_variants = set()
            for variant in valid_variants - constrained_variants:
                matches_true = any(
                    getattr(c, variant) and self.satisfies(c)
                    for c in self._concrete_cls.all_possible()
                )
                matches_false = any(
                    not getattr(c, variant) and self.satisfies(c)
                    for c in self._concrete_cls.all_possible()
                )
                if matches_true and matches_false:
                    bad_variants.add(variant)
            if bad_variants and strict:
                vs = ", ".join(sorted(bad_variants))
                raise ValueError(
                    f"Specification must constrain or explicitly mark unconstrained: {vs}"
                )

        # If required, check that we match *something*
        if not allow_empty:
            if isinstance(self._clauses, bool):
                matches_any = self._clauses
            else:
                matches_any = any(self.satisfies(c) for c in self._concrete_cls.all_possible())
            if not matches_any:
                raise ValueError(f"Specification does not match anything: {spec!r}")

    def satisfies(self, concrete: ConcreteSpecification) -> bool:
        """Test whether the given ConcreteSpecification satisfies this Specification."""
        if isinstance(self._clauses, bool):
            return self._clauses

        for vrs, c in self._clauses.items():
            cnt = sum(1 if getattr(concrete, v) else 0 for v in vrs)
            if cnt < c.min_enabled or c.max_enabled < cnt:
                return False
        return True

    def __str__(self) -> str:
        if isinstance(self._clauses, bool):
            return "all" if self._clauses else "none"

        fragments = []
        for vrs, cond in self._clauses.items():
            if len(vrs) == 1:
                assert cond.min_enabled == cond.max_enabled
                fragments.append(("+" if cond.min_enabled == 1 else "~") + vrs[0])
            else:
                cfrags = []
                if cond.min_enabled > 0:
                    cfrags.append(f"+>{cond.min_enabled:d}")
                if cond.max_enabled < len(vrs):
                    cfrags.append(f"+<{cond.max_enabled:d}")
                assert cfrags
                fragments.append(f"({' '.join(vrs)})[{' '.join(cfrags)}]")
        return " ".join(fragments)

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}({str(self)!r})"


class Configuration:
    """Representation of a possible build configuration of HPCToolkit."""

    args: list[str]

    def __init__(
        self,
        meson: Path,
        args: collections.abc.Iterable[str],
        comp: Compilers,
        variant: ConcreteSpecification,
    ):
        """Derive the full Configuration by adjusting the given args to meson setup to match
        the expected comp(ilers) and variant.
        """
        self.meson = meson

        self.args = [
            *args,
            f"-Dpapi={'enabled' if variant.papi else 'disabled'}",
            f"-Dpython={'enabled' if variant.python else 'disabled'}",
            f"-Dcuda={'enabled' if variant.cuda else 'disabled'}",
            f"-Dlevel0={'enabled' if variant.level0 else 'disabled'}",
            f"-Dgtpin={'enabled' if variant.gtpin else 'disabled'}",
            f"-Dopencl={'enabled' if variant.opencl else 'disabled'}",
            f"-Drocm={'enabled' if variant.rocm else 'disabled'}",
            f"-Dhpcprof_mpi={'enabled' if variant.mpi else 'disabled'}",
            f"-Dbuildtype={'debugoptimized' if variant.debug else 'release'}",
            f"-Dvalgrind_annotations={'true' if variant.valgrind_debug else 'false'}",
        ]

        self.native = MesonMachineFile()
        self.env: collections.abc.MutableMapping[str, str] = collections.ChainMap({}, os.environ)

        # Apply the configuration from the Compilers
        if comp.custom_compilers:
            cc, cxx = comp.custom_compilers
            self.native.add_binary("c", Path(cc))
            self.native.add_binary("cpp", Path(cxx))
            self.env["OMPI_CC"] = cc
            self.env["OMPI_CXX"] = cxx
