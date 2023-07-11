import collections.abc
import functools
import json
import shutil
import typing
from pathlib import Path

import click
from yaspin import yaspin  # type: ignore[import]

from .command import Command, ShEnv
from .meson import MesonMachineFile
from .spack import Spack
from .spec import DependencyMode, SpackEnv, VerConSpec

__all__ = ("AutogenEnv", "DevEnv", "InvalidSpecificationError")


class InvalidSpecificationError(ValueError):
    pass


class AutogenEnv(SpackEnv):
    """Variant of a SpackEnv for the ./autogen environment."""

    _SPECS: typing.ClassVar[frozenset[VerConSpec]] = frozenset(
        {
            VerConSpec("autoconf @2.69"),
            VerConSpec("automake @1.15.1"),
            VerConSpec("libtool @2.4.6"),
            VerConSpec("m4"),
        }
    )

    def environ(self) -> ShEnv:
        overlay = {
            "AUTOCONF": str(self.packages["autoconf"].prefix / "bin" / "autoconf"),
            "ACLOCAL": str(self.packages["automake"].prefix / "bin" / "aclocal"),
            "AUTOHEADER": str(self.packages["autoconf"].prefix / "bin" / "autoheader"),
            "AUTOM4TE": str(self.packages["autoconf"].prefix / "bin" / "autom4te"),
            "AUTOMAKE": str(self.packages["automake"].prefix / "bin" / "automake"),
            "LIBTOOLIZE": str(self.packages["libtool"].prefix / "bin" / "libtoolize"),
            "M4": str(self.packages["m4"].prefix / "bin" / "m4"),
        }
        return ShEnv("").extend(overlay)

    @property
    def autoreconf(self) -> Command:
        return Command(self.packages["autoconf"].prefix / "bin" / "autoreconf")

    def generate(
        self, unresolve: collections.abc.Collection[str] = (), template: dict | None = None
    ) -> None:
        self.generate_explicit(
            self._SPECS, DependencyMode.ANY, unresolve=unresolve, template=template
        )


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
        mpi: bool,
    ) -> None:
        if gtpin and not level0:
            raise InvalidSpecificationError("level0 must be true if gtpin is true")
        super().__init__(root)
        self.cuda = cuda
        self.rocm = rocm
        self.level0 = level0
        self.gtpin = gtpin
        self.opencl = opencl
        self.python = python
        self.papi = papi
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
            "mpi": self.mpi,
        }

    def describe(self) -> str:
        """Generate a multi-line description of this development environment.

        This description is colorized using click.style(). If colors are not desired use click.unstyle().
        """
        yes = click.style("YES", fg="green", bold=True)
        no = click.style("NO", fg="red", bold=True)

        return f"""\
    Compiler             : {self.recommended_compiler.spec if self.recommended_compiler else '(undefined)'}
    hpcprof-mpi          : {yes if self.mpi else no}

  {click.style("CPU Features", bold=True)}
    PAPI                 : {yes if self.papi else no}
    Python               : {yes if self.python else no}

  {click.style("GPU Support", bold=True)}
    NVIDIA / CUDA        : {yes if self.cuda else no}
    AMD / ROCm           : {yes if self.rocm else no}
    Intel / Level Zero   : {yes if self.level0 else no}
    Intel Instrumentation: {yes if self.gtpin else no}
"""

    @functools.cached_property
    def _py_specs(self) -> set[VerConSpec]:
        """Generate the set of Python extensions that need to be installed, as VerConSpecs."""
        return {
            VerConSpec("py-poetry-core @1.2.0:"),
            VerConSpec("py-pytest @7.2.1:"),
            VerConSpec("py-ruamel-yaml @0.17.16:"),
            VerConSpec("py-click @8.1.3:"),
            VerConSpec("py-pyparsing @3.0.9:"),
        }

    @functools.cached_property
    def _specs(self) -> set[VerConSpec]:
        """Generate the set of Spack specs that will need to be installed for the given config, as VerConSpecs."""
        result: set[VerConSpec] = (
            {
                # NB: Meson is required as a build tool, but does not need to be included in the devenv
                # itself. It is instead installed separately in a venv by __main__.py.
                VerConSpec("autoconf @2.69"),
                VerConSpec("automake @1.15.1"),
                VerConSpec("libtool @2.4.6"),
                VerConSpec("m4"),
                VerConSpec("gmake"),
                VerConSpec(
                    """boost @1.70.0:
                +atomic +date_time +filesystem +system +thread +timer +graph +regex
                +shared +multithreaded
                visibility=global"""
                ),
                VerConSpec("bzip2 @1.0.8:"),
                VerConSpec("dyninst @12.3.0:"),
                VerConSpec("elfutils ~nls @0.186:"),
                VerConSpec("intel-tbb +shared @2020.3"),
                VerConSpec("libmonitor +hpctoolkit ~dlopen @2023.03.15:"),
                VerConSpec("xz +pic libs=static @5.2.5:5.2.6,5.2.10:"),
                VerConSpec("zlib +shared @1.2.13:"),
                VerConSpec("libunwind +xz +pic @1.6.2:"),
                VerConSpec("libpfm4 @4.11.0:"),
                VerConSpec("xerces-c transcoder=iconv @3.2.3:"),
                VerConSpec("libiberty +pic @2.37:"),
                VerConSpec("intel-xed +pic @2022.04.17:", when="arch.satisfies('target=x86_64:')"),
                VerConSpec("memkind"),
                VerConSpec("yaml-cpp +shared @0.7.0:"),
                VerConSpec("pkgconfig"),
                VerConSpec("python +ctypes +lzma +bz2 +zlib @3.10.8:"),
            }
            | self._py_specs
        )

        if self.papi:
            result.add(VerConSpec("papi @6.0.0.1:"))
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

    def generate(
        self,
        mode: DependencyMode,
        unresolve: collections.abc.Collection[str] = (),
        template: dict | None = None,
    ) -> None:
        self.generate_explicit(self._specs, mode, unresolve=unresolve, template=template)
        with open(self.root / "dev.json", "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f)

    def _populate_pyview(self) -> None:
        """Populate the pyview with all Python extensions. Basically an expensive venv."""
        if self._pyview.exists():
            shutil.rmtree(self._pyview)

        packages = set()
        for s in self._py_specs:
            pkg = self.packages[s.package]
            packages.add(pkg)
            packages |= pkg.dependencies
        Spack.get().view_add(
            self._pyview, *sorted(packages, key=lambda p: p.fullhash), spack_env=self.root
        )

    def _m_binaries(self, native: MesonMachineFile) -> None:
        assert self.recommended_compiler
        native.add_binary("c", self.recommended_compiler.cc)
        native.add_binary("cpp", self.recommended_compiler.cpp)
        native.add_binary("autoreconf", self.packages["autoconf"].prefix / "bin" / "autoreconf")
        native.add_binary("autoconf", self.packages["autoconf"].prefix / "bin" / "autoconf")
        native.add_binary("aclocal", self.packages["automake"].prefix / "bin" / "aclocal")
        native.add_binary("autoheader", self.packages["autoconf"].prefix / "bin" / "autoheader")
        native.add_binary("autom4te", self.packages["autoconf"].prefix / "bin" / "autom4te")
        native.add_binary("automake", self.packages["automake"].prefix / "bin" / "automake")
        native.add_binary("libtoolize", self.packages["libtool"].prefix / "bin" / "libtoolize")
        native.add_binary("m4", self.packages["m4"].prefix / "bin" / "m4")
        native.add_binary("make", self.packages["gmake"].prefix / "bin" / "make")
        native.add_binary("python3", self._pyview / "bin" / "python3")
        native.add_binary("python", self._pyview / "bin" / "python3")
        if self.mpi:
            native.add_binary("mpicxx", self.which("mpicxx", VerConSpec("mpi")).path)
            native.add_binary("mpiexec", self.which("mpiexec", VerConSpec("mpi")).path)
        if self.cuda:
            native.add_binary("cuda", self.which("nvcc", VerConSpec("cuda")).path)
            native.add_binary("nvdisasm", self.which("nvdisasm", VerConSpec("cuda")).path)

    def _m_prefixes(self, native: MesonMachineFile) -> None:
        native.add_property("prefix_boost", self.packages["boost"].prefix)
        native.add_property("prefix_bzip", self.packages["bzip2"].prefix)
        native.add_property("prefix_dyninst", self.packages["dyninst"].prefix)
        native.add_property("prefix_elfutils", self.packages["elfutils"].prefix)
        native.add_property("prefix_tbb", self.packages["intel-tbb"].prefix)
        native.add_property("prefix_libmonitor", self.packages["libmonitor"].prefix)
        native.add_property("prefix_libunwind", self.packages["libunwind"].prefix)
        native.add_property("prefix_perfmon", self.packages["libpfm4"].prefix)
        native.add_property("prefix_xerces", self.packages["xerces-c"].prefix)
        native.add_property("prefix_lzma", self.packages["xz"].prefix)
        native.add_property("prefix_zlib", self.packages["zlib"].prefix)
        native.add_property("prefix_libiberty", self.packages["libiberty"].prefix)
        if "intel-xed" in self.packages:
            native.add_property("prefix_xed", self.packages["intel-xed"].prefix)
        native.add_property("prefix_memkind", self.packages["memkind"].prefix)
        native.add_property("prefix_yaml_cpp", self.packages["yaml-cpp"].prefix)
        if self.papi:
            native.add_property("prefix_papi", self.packages["papi"].prefix)
        if self.opencl:
            native.add_property("prefix_opencl", self.packages["opencl-c-headers"].prefix)
        if self.gtpin:
            native.add_property("prefix_gtpin", self.packages["intel-gtpin"].prefix)
            native.add_property("prefix_igc", self.packages["oneapi-igc"].prefix)
        if self.level0 or self.gtpin:
            native.add_property("prefix_level0", self.packages["oneapi-level-zero"].prefix)
        if self.rocm:
            native.add_property("prefix_rocm_hip", self.packages["hip"].prefix)
            native.add_property("prefix_rocm_hsa", self.packages["hsa-rocr-dev"].prefix)
            native.add_property("prefix_rocm_tracer", self.packages["roctracer-dev"].prefix)
            native.add_property("prefix_rocm_profiler", self.packages["rocprofiler-dev"].prefix)
        if self.cuda:
            native.add_property("prefix_cuda", self.packages["cuda"].prefix)

    def populate(self) -> None:
        """Populate the development environment with all the files needed to use it."""
        self._populate_pyview()

        native = MesonMachineFile()
        self._m_binaries(native)
        self._m_prefixes(native)
        # FIXME: Eventually we should set the defaults for the feature options here in the native
        # file, but that tends to interfere with options specified on the command line.
        # So for now we just set them via the initial command line as well.
        # Tracking issue: https://github.com/mesonbuild/meson/issues/11930
        native_path = self.root / "meson_native.ini"
        native.save(native_path)

    def setup(self, dev_root: Path, meson: Command) -> None:
        """Set up the build directory for this development environment. Must already be populated."""
        native_path = self.root / "meson_native.ini"
        assert native_path.is_file()

        self.builddir.mkdir(exist_ok=True)
        with yaspin(text="Configuring build directory"):
            meson(
                "setup",
                "--wipe",
                f"--native-file={native_path}",
                f"--prefix={self.installdir}",
                f"-Dhpcprof_mpi={'enabled' if self.mpi else 'disabled'}",
                f"-Dpapi={'enabled' if self.papi else 'disabled'}",
                f"-Dpython={'enabled' if self.python else 'disabled'}",
                f"-Dopencl={'enabled' if self.opencl else 'disabled'}",
                f"-Dlevel0={'enabled' if self.level0 else 'disabled'}",
                f"-Dgtpin={'enabled' if self.gtpin else 'disabled'}",
                f"-Drocm={'enabled' if self.rocm else 'disabled'}",
                f"-Dcuda={'enabled' if self.cuda else 'disabled'}",
                self.builddir,
                dev_root,
                output=False,
            )

    @property
    def _pyview(self) -> Path:
        return self.root / "_pyview"

    @property
    def builddir(self) -> Path:
        return self.root / "builddir"

    @property
    def installdir(self) -> Path:
        return self.root / "installdir"

    def build(self, meson: Command) -> None:
        with yaspin(text="Building HPCToolkit"):
            meson("compile", cwd=self.builddir, output=False)

        with yaspin(text="Installing HPCToolkit"):
            meson("install", cwd=self.builddir, output=False)
