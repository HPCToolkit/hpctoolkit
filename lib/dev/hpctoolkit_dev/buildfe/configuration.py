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
import textwrap
import typing
from pathlib import Path

from .logs import FgColor, colorize


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


class _ManifestFile:
    def __init__(self, path):
        self.path = Path(path)

    def check(self, installdir) -> tuple[set[Path], set[Path | tuple[Path, str]]]:
        if (Path(installdir) / self.path).is_file():
            return {self.path}, set()
        return set(), {self.path}


class _ManifestLib(_ManifestFile):
    def __init__(self, path, target, *aliases):
        super().__init__(path)
        self.target = str(target)
        self.aliases = [str(a) for a in aliases]

    def check(self, installdir) -> tuple[set[Path], set[Path | tuple[Path, str]]]:
        installdir = Path(installdir)
        found, missing = set(), set()

        target = installdir / self.path
        target = target.with_name(target.name + self.target)
        if not target.is_file():
            missing.add(target.relative_to(installdir))
        if target.is_symlink():
            missing.add(
                (target.relative_to(installdir), f"Unexpected symlink to {os.readlink(target)}")
            )

        for a in self.aliases:
            alias = installdir / self.path
            alias = alias.with_name(alias.name + a)
            if not alias.is_file():
                missing.add(alias.relative_to(installdir))
                continue
            if not alias.is_absolute():
                missing.add((alias.relative_to(installdir), "Not a symlink"))
                continue

            targ = Path(os.readlink(alias))
            if len(targ.parts) > 1:
                missing.add(
                    (alias.relative_to(installdir), "Invalid symlink, must point to sibling file")
                )
                continue
            if targ.name != target.name:
                missing.add(
                    (alias.relative_to(installdir), f"Invalid symlink, must point to {target.name}")
                )
                continue

            found.add(alias.relative_to(installdir))

        return found, missing


class _ManifestExtLib(_ManifestFile):
    def __init__(self, path, main_suffix, *suffixes):
        super().__init__(path)
        self.main_suffix = str(main_suffix)
        self.suffixes = [str(s) for s in suffixes]

    def check(self, installdir) -> tuple[set[Path], set[Path | tuple[Path, str]]]:
        installdir = Path(installdir)
        common = installdir / self.path
        found = set()

        main_path = common.with_name(common.name + self.main_suffix)
        if not main_path.is_file():
            return set(), {main_path.relative_to(installdir)}
        found.add(main_path.relative_to(installdir))

        for path in common.parent.iterdir():
            if path.name.startswith(common.name) and path != main_path:
                name = path.name[len(common.name) :]
                if any(re.match(s, name) for s in self.suffixes):
                    found.add(path.relative_to(installdir))

        return found, set()


class Manifest:
    """Representation of an install manifest."""

    def __init__(self, *, mpi: bool):
        """Given a set of variant-keywords, determine the install manifest as a list of ManifestFiles."""

        def so(n):
            return r"\.so" + r"\.\d+" * n.__index__()

        self.files = [
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_atomic-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_atomic", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_chrono", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_date_time-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_date_time", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_filesystem-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_filesystem", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_graph-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_graph", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_regex-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_regex", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_system-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_system", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_thread-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_thread", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_timer-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_timer", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libbz2", ".so", so(1), so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libcommon", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libdw", ".so", so(1), r"-\d+\.\d+\.so"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libdynDwarf", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libdynElf", ".so", so(1), r"-\d+\.\d+\.so"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libelf", ".so", so(1), r"-\d+\.\d+\.so"),
            _ManifestExtLib(
                "lib/hpctoolkit/ext-libs/libinstructionAPI", ".so", so(1), r"-\d+\.\d+\.so"
            ),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libmonitor_wrap", ".a"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libmonitor", ".so", ".so.0", ".so.0.0.0"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libparseAPI", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libpfm", ".so", so(1), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libsymtabAPI", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libtbb", ".so", so(1)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libtbbmalloc_proxy", ".so", so(1)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libtbbmalloc", ".so", so(1)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libxerces-c", ".a"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libxerces-c", ".so", r"-\d+.\d+\.so"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libz", ".a"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libz", ".so", so(1), so(3)),
            _ManifestFile("bin/hpclink"),
            _ManifestFile("bin/hpcprof"),
            _ManifestFile("bin/hpcrun"),
            _ManifestFile("bin/hpcstruct"),
            _ManifestFile("include/hpctoolkit.h"),
            _ManifestFile("lib/hpctoolkit/hash-file"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_audit.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_audit.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_dlmopen.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_dlmopen.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_fake_audit.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_fake_audit.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_ga.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_ga.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_gprof.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_gprof.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_io.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_io.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_memleak.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_memleak.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_pthread.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_pthread.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun.o"),
            _ManifestFile("lib/hpctoolkit/libhpcrun.so"),
            # XXX: Why is there no libhpcrun.so.0.0.0 and libhpcrun.la?
            _ManifestFile("lib/hpctoolkit/libhpctoolkit.a"),
            _ManifestFile("lib/hpctoolkit/libhpctoolkit.la"),
            _ManifestFile("lib/hpctoolkit/plugins/ga"),
            _ManifestFile("lib/hpctoolkit/plugins/io"),
            _ManifestFile("lib/hpctoolkit/plugins/memleak"),
            _ManifestFile("lib/hpctoolkit/plugins/pthread"),
            # XXX: Why is there no gprof?
            _ManifestFile("libexec/hpctoolkit/config.guess"),
            _ManifestFile("libexec/hpctoolkit/dotgraph-bin"),
            _ManifestFile("libexec/hpctoolkit/dotgraph"),
            _ManifestFile("libexec/hpctoolkit/hpcfnbounds"),
            _ManifestFile("libexec/hpctoolkit/hpcguess"),
            _ManifestFile("libexec/hpctoolkit/hpclog"),
            _ManifestFile("libexec/hpctoolkit/hpcplatform"),
            _ManifestFile("libexec/hpctoolkit/hpcproftt-bin"),
            _ManifestFile("libexec/hpctoolkit/hpcproftt"),
            _ManifestFile("libexec/hpctoolkit/hpcsummary"),
            _ManifestFile("libexec/hpctoolkit/hpctracedump"),
            _ManifestFile("share/doc/hpctoolkit/documentation.html"),
            _ManifestFile("share/doc/hpctoolkit/download.html"),
            _ManifestFile("share/doc/hpctoolkit/examples.html"),
            _ManifestFile("share/doc/hpctoolkit/fig/hpctoolkit-workflow.png"),
            _ManifestFile("share/doc/hpctoolkit/fig/hpcviewer-annotated-screenshot.jpg"),
            _ManifestFile("share/doc/hpctoolkit/fig/index.html"),
            _ManifestFile("share/doc/hpctoolkit/fig/spacer.gif"),
            _ManifestFile("share/doc/hpctoolkit/FORMATS.md"),
            _ManifestFile("share/doc/hpctoolkit/METRICS.yaml"),
            _ManifestFile("share/doc/hpctoolkit/googleeeb6a75d4102e1ef.html"),
            _ManifestFile("share/doc/hpctoolkit/hpctoolkit.org.sitemap.txt"),
            _ManifestFile("share/doc/hpctoolkit/index.html"),
            _ManifestFile("share/doc/hpctoolkit/info-acks.html"),
            _ManifestFile("share/doc/hpctoolkit/info-people.html"),
            _ManifestFile("share/doc/hpctoolkit/LICENSE"),
            _ManifestFile("share/doc/hpctoolkit/man/hpclink.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcprof.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcproftt.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcrun.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcstruct.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpctoolkit.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcviewer.html"),
            _ManifestFile("share/doc/hpctoolkit/manual/HPCToolkit-users-manual.pdf"),
            _ManifestFile("share/doc/hpctoolkit/overview.html"),
            _ManifestFile("share/doc/hpctoolkit/publications.html"),
            _ManifestFile("share/doc/hpctoolkit/README.Acknowledgments"),
            _ManifestFile("share/doc/hpctoolkit/README.Install"),
            _ManifestFile("share/doc/hpctoolkit/README.md"),
            _ManifestFile("share/doc/hpctoolkit/README.ReleaseNotes"),
            _ManifestFile("share/doc/hpctoolkit/software-instructions.html"),
            _ManifestFile("share/doc/hpctoolkit/software.html"),
            _ManifestFile("share/doc/hpctoolkit/spack-issues.html"),
            _ManifestFile("share/doc/hpctoolkit/style/footer-hpctoolkit.js"),
            _ManifestFile("share/doc/hpctoolkit/style/header-hpctoolkit.js"),
            _ManifestFile("share/doc/hpctoolkit/style/header.gif"),
            _ManifestFile("share/doc/hpctoolkit/style/index.html"),
            _ManifestFile("share/doc/hpctoolkit/style/style.css"),
            _ManifestFile("share/doc/hpctoolkit/training.html"),
            _ManifestFile("share/hpctoolkit/dtd/hpc-experiment.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/hpc-structure.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/hpcprof-config.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsa.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsb.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsc.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsn.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamso.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsr.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isobox.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isocyr1.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isocyr2.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isodia.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isogrk3.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isolat1.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isolat2.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isomfrk.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isomopf.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isomscr.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isonum.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isopub.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isotech.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/mathml.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/mmlalias.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/mmlextra.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/xhtml1-transitional-mathml.dtd"),
            _ManifestFile("share/man/man1/hpclink.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcprof.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcproftt.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcrun.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcstruct.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpctoolkit.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcviewer.1hpctoolkit"),
            _ManifestLib("lib/hpctoolkit/libhpcrun_audit.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_dlmopen.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_fake_audit.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_ga.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_gprof.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_io.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_memleak.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_pthread.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpctoolkit.so", ".0.0.0", ".0", ""),
        ]

        if mpi:
            self.files += [
                _ManifestFile("bin/hpcprof-mpi"),
            ]

    def check(self, installdir: Path) -> tuple[int, int]:
        """Scan an install directory and compare against the expected manifest. Prints the results
        of the checks to the log. Return the counts of missing and unexpected files.
        """
        # First derive the full listing of actually installed files
        listing = set()
        for root, _, files in os.walk(installdir):
            for filename in files:
                listing.add((Path(root) / filename).relative_to(installdir))

        # Then match these files up with the results we found
        n_unexpected = 0
        n_uninstalled = 0
        warnings: list[str] = []
        errors: list[str] = []
        for f in self.files:
            found, not_found = f.check(installdir)
            warnings.extend(f"+ {fn.as_posix()}" for fn in found - listing)
            n_unexpected += len(found - listing)
            listing -= found
            for fn in not_found:
                if isinstance(fn, tuple):
                    fn, msg = fn
                    errors.append(f"! {fn.as_posix()}\n  ^ {textwrap.indent(msg, '    ')}")
                else:
                    errors.append(f"- {fn.as_posix()}")
            n_uninstalled += len(not_found)

        # Print out the warnings and then the errors, with colors
        with colorize(FgColor.warning):
            for hunk in warnings:
                print(hunk)
        with colorize(FgColor.error):
            for hunk in errors:
                print(hunk)

        return n_uninstalled, n_unexpected


@dataclasses.dataclass(kw_only=True, frozen=True)
class ConcreteSpecification:
    """Point in the build configuration space, represented as a series of boolean-valued variants."""

    tests2: bool
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
            ("tests2", True, False),
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
        self, args: collections.abc.Iterable[str], comp: Compilers, variant: ConcreteSpecification
    ):
        """Derive the full Configuration by adjusting the given args to ./configure to match
        the expected comp(ilers) and variant.
        """
        make = shutil.which("make")
        if make is None:
            raise RuntimeError("Unable to find make!")
        self.make: str = make

        self.manifest: Manifest = Manifest(mpi=variant.mpi)

        # Transform the example arguments as requested
        self.args = self._args_with_if(args, variant.papi, "papi")
        if variant.papi:
            # Elide any --with-perfmon so libpfm comes from --with-papi
            self.args = [a for a in self.args if not a.startswith("--with-perfmon=")]
        else:
            # Ensure there's a --with-perfmon available since we're --without-papi
            self.args = self._transform_args(self.args, "--with-perfmon=", None)
        self.args = self._args_with_if(self.args, variant.python, "python")
        self.args = self._args_with_if(self.args, variant.cuda, "cuda")
        self.args = self._args_with_if(self.args, variant.level0, "level0")
        self.args = self._args_with_if(self.args, variant.gtpin, "gtpin")
        self.args = self._args_with_if(self.args, variant.gtpin, "igc")
        self.args = self._args_with_if(self.args, variant.opencl, "opencl")
        self.args = self._args_with_if(
            self.args, variant.rocm, "rocm", "rocm-hip", "rocm-hsa", "rocm-tracer", "rocm-profiler"
        )
        self.args.extend([f"MPI{cc}=" for cc in ("CC", "F77")])
        if variant.mpi:
            self.args = self._transform_args(self.args, "MPICXX=", None)
        else:
            self.args = [a for a in self.args if not a.startswith("MPICXX=")]
            self.args.append("MPICXX=")
        self.args = self._args_enable_if(self.args, variant.debug, "develop")
        self.args = self._args_enable_if(self.args, variant.valgrind_debug, "valgrind-annotations")
        self.args = self._args_enable_if(self.args, variant.tests2, "tests2")

        self.env: collections.abc.MutableMapping[str, str] = collections.ChainMap({}, os.environ)

        # Apply the configuration from the Compilers
        if comp.custom_compilers:
            cc, cxx = comp.custom_compilers
            self.args.append(f"CC={cc}")
            self.args.append(f"CXX={cxx}")
            self.env["OMPI_CC"] = cc
            self.env["OMPI_CXX"] = cxx

    @staticmethod
    def _transform_args(
        args: collections.abc.Iterable[str],
        last_of: str,
        none_of: str | None,
        *,
        add_if_missing: bool = False,
    ) -> list[str]:
        result_pre: list[str] = []
        main_arg: str | None = None
        result_post: list[str] = []
        for a in args:
            if none_of is not None and a.startswith(none_of):
                continue

            if a.startswith(last_of):
                main_arg = a
                result_pre.extend(result_post)
                result_post = []
                continue

            (result_pre if main_arg is None else result_post).append(a)

        if main_arg is None:
            if not add_if_missing:
                raise UnsatisfiableSpecError(last_of)
            main_arg = last_of

        return [*result_pre, main_arg, *result_post]

    @classmethod
    def _args_with_if(
        cls, args: collections.abc.Iterable[str], cond: bool, *packages: str
    ) -> list[str]:
        result = list(args)
        found = False
        for p in packages:
            with contextlib.suppress(UnsatisfiableSpecError):
                result = (
                    cls._transform_args(result, f"--with-{p}=", f"--without-{p}")
                    if cond
                    else cls._transform_args(
                        result, f"--without-{p}", f"--with-{p}=", add_if_missing=True
                    )
                )
                found = True
        if not found:
            if not packages:
                raise ValueError
            raise UnsatisfiableSpecError(packages[0])
        return result

    @classmethod
    def _args_enable_if(
        cls, args: collections.abc.Iterable[str], cond: bool, feature: str
    ) -> list[str]:
        inc = f"--enable-{feature}" if cond else f"--disable-{feature}"
        exc = f"--disable-{feature}" if cond else f"--enable-{feature}"
        return cls._transform_args(args, inc, exc, add_if_missing=True)
