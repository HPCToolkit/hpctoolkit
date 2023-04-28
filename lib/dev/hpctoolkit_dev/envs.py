import contextlib
import functools
import json
import os
import shlex
import stat
import typing
from pathlib import Path

import click
from yaspin import yaspin  # type: ignore[import]

from .command import Command, ShEnv
from .spack import Spack
from .spec import DependencyMode, SpackEnv, VerConSpec

__all__ = ("AutogenEnv", "DevEnv")


class AutogenEnv(SpackEnv):
    """Variant of a SpackEnv for the ./autogen environment."""

    _SPECS: typing.ClassVar[frozenset[VerConSpec]] = frozenset(
        {
            VerConSpec("autoconf @2.69"),
            VerConSpec("automake @1.15.1"),
            VerConSpec("libtool @2.4.6"),
        }
    )
    packages: typing.ClassVar[frozenset[str]] = frozenset(s.package for s in _SPECS)

    def load_all(self) -> ShEnv:
        return self.load(*self.packages)

    def environ(self) -> ShEnv:
        overlay = {
            cmd.upper(): str(self.which(cmd).path)
            for cmd in (
                "autoconf",
                "aclocal",
                "autoheader",
                "autom4te",
                "automake",
                "libtoolize",
                "m4",
            )
        }
        return self.load_all().extend(overlay)

    @property
    def autoreconf(self) -> Command:
        return self.which("autoreconf", "autoconf")

    def generate(self) -> None:
        self.generate_explicit(self._SPECS, DependencyMode.ANY)


class DevEnv(SpackEnv):
    """Variant of a SpackEnv for development environments (devenvs)."""

    @classmethod
    def default(cls, feature: str) -> bool:
        """Determine the default setting for an 'auto' feature."""
        match feature:
            case "mpi":
                return False  # Too much trouble to autoguess
            case "papi" | "python" | "opencl":
                return True
            case "cuda":
                return bool(Spack.get().fetch_external_versions("cuda"))
            case "rocm":
                return bool(Spack.get().fetch_external_versions("hip"))
            case "level0":
                return bool(Spack.get().fetch_external_versions("oneapi-level-zero"))
            case "gtpin":
                return bool(Spack.get().fetch_external_versions("intel-gtpin"))
            case _:
                raise ValueError(feature)

    def __init__(
        self,
        root: Path,
        *,
        cuda: bool,
        rocm: bool,
        level0: bool,
        gtpin: bool,
        opencl: bool,
        python: bool,
        papi: bool,
        optional_papi: bool,
        mpi: bool,
    ) -> None:
        super().__init__(root)
        self.cuda = cuda
        self.rocm = rocm
        self.level0 = level0
        self.gtpin = gtpin
        self.opencl = opencl
        self.python = python
        self.papi = papi
        self.optional_papi = optional_papi
        self.mpi = mpi

    @classmethod
    def restore(cls, root: Path) -> "DevEnv":
        with open(root / "dev.json", encoding="utf-8") as f:
            data = json.load(f)

        def flag(key: str) -> bool:
            if key in data and not isinstance(data[key], bool):
                raise TypeError(data[key])
            return data.get(key, False)

        return cls(
            root,
            cuda=flag("cuda"),
            rocm=flag("rocm"),
            level0=flag("level0"),
            gtpin=flag("gtpin"),
            opencl=flag("opencl"),
            python=flag("python"),
            papi=flag("papi"),
            optional_papi=flag("optional_papi"),
            mpi=flag("mpi"),
        )

    def to_dict(self) -> dict[str, bool | str | None]:
        return {
            "cuda": self.cuda,
            "rocm": self.rocm,
            "level0": self.level0,
            "gtpin": self.gtpin,
            "opencl": self.opencl,
            "python": self.python,
            "papi": self.papi,
            "optional_papi": self.optional_papi,
            "mpi": self.mpi,
        }

    def describe(self) -> str:
        """Generate a multi-line description of this development environment.

        This description is colorized using click.style(). If colors are not desired use click.unstyle().
        """
        yes = click.style("YES", fg="green", bold=True)
        no = click.style("NO", fg="red", bold=True)

        return f"""\
    hpcprof-mpi          : {yes if self.mpi else no}

  {click.style("CPU Features", bold=True)}
    PAPI                 : {"BOTH" if self.optional_papi else yes if self.papi else no}
    Python               : {yes if self.python else no}

  {click.style("GPU Support", bold=True)}
    NVIDIA / CUDA        : {yes if self.cuda else no}
    AMD / ROCm           : {yes if self.rocm else no}
    Intel / Level Zero   : {yes if self.level0 else no}
    Intel Instrumentation: {yes if self.gtpin else no}
"""

    @functools.cached_property
    def _specs(self) -> set[VerConSpec]:
        """Generate the set of Spack specs that will need to be installed for the given config, as VerConSpecs."""
        result: set[VerConSpec] = {
            VerConSpec(
                """boost @1.70.0:
                +atomic +date_time +filesystem +system +thread +timer +graph +regex
                +shared +multithreaded
                visibility=global"""
            ),
            VerConSpec("bzip2 @1.0.8:"),
            VerConSpec("dyninst @12.3.0:"),
            VerConSpec("elfutils +bzip2 +xz ~nls @0.186:"),
            VerConSpec("intel-tbb +shared @2020.3"),
            VerConSpec("libmonitor +hpctoolkit ~dlopen @2022.09.02"),
            VerConSpec("xz +pic libs=static @5.2.5:5.2.6,5.2.10:"),
            VerConSpec("zlib +shared @1.2.13:"),
            VerConSpec("libunwind +xz +pic @1.6.2:"),
            VerConSpec("xerces-c transcoder=iconv @3.2.3:"),
            VerConSpec("libiberty +pic @2.37:"),
            VerConSpec("intel-xed +pic @2022.04.17:", when="arch.satisfies('target=x86_64:')"),
            VerConSpec("memkind"),
            VerConSpec("yaml-cpp +shared @0.7.0:"),
            VerConSpec("python +ctypes +lzma +bz2 +zlib @3.10.8:"),
            VerConSpec("meson @1.0.0:"),
            VerConSpec("pkgconfig"),
            VerConSpec("py-poetry-core @1.2.0:"),
            VerConSpec("py-pytest @7.2.1:"),
            VerConSpec("py-ruamel-yaml @0.17.16:"),
            VerConSpec("py-click @8.1.3:"),
            VerConSpec("py-pyparsing @3.0.9:"),
        }

        if self.papi or self.optional_papi:
            result.add(VerConSpec("papi @6.0.0.1:"))
        if not self.papi or self.optional_papi:
            result.add(VerConSpec("libpfm4 @4.11.0:"))
        if self.cuda:
            result.add(VerConSpec("cuda @10.2.89:"))
        if self.level0 or self.gtpin:
            result.add(VerConSpec("oneapi-level-zero @1.7.15:"))
        if self.gtpin:
            result.add(VerConSpec("intel-gtpin @3.0:"))
            result.add(VerConSpec("oneapi-igc"))
        if self.opencl:
            result.add(VerConSpec("opencl-c-headers @2022.01.04:"))
        if self.rocm:
            result.add(VerConSpec("hip @5.1.3:"))
            result.add(VerConSpec("hsa-rocr-dev @5.1.3:"))
            result.add(VerConSpec("roctracer-dev @5.1.3:"))
            result.add(VerConSpec("rocprofiler-dev @5.1.3:"))
        if self.mpi:
            result.add(VerConSpec("mpi"))

        return result

    @property
    def packages(self) -> set[str]:
        return {s.package for s in self._specs if self.has_package(s.package)}

    def load_all(self) -> ShEnv:
        return self.load(*self.packages)

    def generate(self, mode: DependencyMode) -> None:
        self.generate_explicit(self._specs, mode)
        with open(self.root / "dev.json", "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f)

    def _pop_base(self) -> list[str]:
        result: list[str] = []

        result.append(f"--prefix={self.installdir}")

        def _with(include: bool, name: str, pkg: str, path: str | os.PathLike = ""):
            if include:
                result.append(f"--with-{name}={self.prefix(pkg) / path}")
            else:
                result.append(f"--without-{name}")

        _with(True, "boost", "boost")
        _with(True, "bzip", "bzip2")
        _with(True, "dyninst", "dyninst")
        _with(True, "elfutils", "elfutils")
        _with(True, "tbb", "intel-tbb")
        _with(True, "libmonitor", "libmonitor")
        _with(True, "libunwind", "libunwind")
        _with(True, "xerces", "xerces-c")
        _with(True, "lzma", "xz")
        _with(True, "zlib", "zlib")
        _with(True, "libiberty", "libiberty")
        if self.has_package("intel-xed"):
            _with(True, "xed", "intel-xed")
        _with(True, "memkind", "memkind")
        _with(True, "yaml-cpp", "yaml-cpp")
        if self.mpi:
            result.append("MPICXX=mpicxx")
        if self.papi or self.optional_papi:
            _with(True, "papi", "papi")
        if not self.papi or self.optional_papi:
            _with(True, "perfmon", "libpfm4")
        _with(self.python, "python", "python", Path("bin/python-config"))
        _with(self.opencl, "opencl", "opencl-c-headers")
        _with(self.gtpin, "gtpin", "intel-gtpin")
        _with(self.gtpin, "igc", "oneapi-igc")
        _with(self.level0 or self.gtpin, "level0", "oneapi-level-zero")
        _with(self.rocm, "rocm-hip", "hip")
        _with(self.rocm, "rocm-hsa", "hsa-rocr-dev")
        _with(self.rocm, "rocm-tracer", "roctracer-dev")
        _with(self.rocm, "rocm-profiler", "rocprofiler-dev")
        _with(self.cuda, "cuda", "cuda")

        return result

    def _pop_meson(self) -> list[str]:
        result: list[str] = ["--enable-tests2"]

        binaries: dict[str, str | Path] = {}
        btoptions: dict[str, str | Path] = {}

        result_file = self.root / "meson_native.ini"
        with open(result_file, "w", encoding="utf-8") as f:
            if binaries:
                f.write("[binaries]\n")
                for what, where in binaries.items():
                    f.write(f"{what} = '{where}'\n")
            if btoptions:
                f.write("[built-in options]\n")
                for what, value in btoptions.items():
                    f.write(f"{what} = '{value}'\n")
        result.append(f"MESON_NATIVE_FILE={result_file}")

        return result

    def populate(self, dev_root: Path | None) -> None:
        """Populate the development environment with all the files needed to use it."""
        cargs = self._pop_base() + self._pop_meson()

        base_configure = dev_root / "configure" if dev_root is not None else "$HPCTOOLKIT/configure"

        config = self.root / "configure"
        with open(config, "w", encoding="utf-8") as f:
            f.write("#!/bin/sh\n")
            f.write(shlex.join(["exec", str(base_configure)]) + " \\\n")
            for arg in cargs:
                f.write(f"  {shlex.quote(arg)} \\\n")
            f.write('  "$@"\n')
        config.chmod(config.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

        if dev_root is not None:
            self.builddir.mkdir(exist_ok=True)
            with self.activate(), yaspin(text="Configuring build directory"):
                self.configure(cwd=self.builddir, output=False)

    @contextlib.contextmanager
    def activate(self) -> typing.Iterator[None]:
        with self.load_all().activate():
            yield

    @property
    def configure(self) -> Command:
        return Command(self.root / "configure")

    @property
    def builddir(self) -> Path:
        return self.root / "builddir"

    @property
    def installdir(self) -> Path:
        return self.root / "installdir"

    def build(self) -> None:
        make = Command.which("make")

        with yaspin(text="Building HPCToolkit"):
            make("-j", "V=0", cwd=self.builddir, output=False)

        with yaspin(text="Installing HPCToolkit"):
            make("-j", "install", cwd=self.builddir, output=False)
