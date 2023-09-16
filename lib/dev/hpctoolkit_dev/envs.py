import collections.abc
import copy
import enum
import functools
import json
import os
import shutil
import subprocess
import textwrap
import typing
from pathlib import Path

import click
import ruamel.yaml
import spiqa
import spiqa.live
import spiqa.syntax as sp_syntax
import spiqa.untrusted
from yaspin import yaspin  # type: ignore[import]

from .command import Command
from .meson import MesonMachineFile
from .spack import Compiler, Spack, preferred_compiler

__all__ = ("DependencyMode", "DevEnv", "InvalidSpecificationError")


@enum.unique
class DependencyMode(enum.Enum):
    LATEST = "--latest"
    MINIMUM = "--minimum"
    ANY = "--any"


class InvalidSpecificationError(ValueError):
    pass


class DevEnv:
    """Variant of a SpackEnv for development environments (devenvs)."""

    @classmethod
    def default(cls, feature: str) -> bool:
        """Determine the default setting for an 'auto' feature."""
        spack = spiqa.live_obj()
        match feature:
            case "mpi":
                return False  # Too much trouble to autoguess
            case "papi" | "python" | "opencl":
                return True
            case "cuda":
                return bool(spack.external_versions("cuda"))
            case "rocm":
                return bool(spack.external_versions("hip"))
            case "level0":
                return bool(spack.external_versions("oneapi-level-zero"))
            case "gtpin":
                return bool(spack.external_versions("intel-gtpin"))
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
        self.root = root.resolve()
        if self.root.exists() and not self.root.is_dir():
            raise FileExistsError(self.root)
        self._package_cache: dict[str, spiqa.query.Package | None] = {}
        self.cuda = cuda
        self.rocm = rocm
        self.level0 = level0
        self.gtpin = gtpin
        self.opencl = opencl
        self.python = python
        self.papi = papi
        self.mpi = mpi

    @functools.cached_property
    def spack(self) -> Spack:
        return Spack(self.root)

    @functools.cached_property
    def sp_spack(self) -> spiqa.Spack:
        return spiqa.query_obj(env=self.root)

    @functools.cached_property
    def sp_livespack(self) -> spiqa.LiveSpack:
        return spiqa.live_obj(env=self.root)

    @staticmethod
    def validate_name(_ctx, _param, name: str) -> str:
        if os.sep in name:
            raise click.BadParameter("slashes are invalid in environment names")
        if name[0] == "_":
            raise click.BadParameter("names starting with '_' are reserved for internal use")
        return name

    def exists(self) -> bool:
        """Run some basic validation on the environment to determine if it even exists yet."""
        return (
            self.root.is_dir()
            and (self.root / "spack.yaml").is_file()
            and (self.root / "spack.lock").is_file()
        )

    @property
    def recommended_compiler(self) -> Compiler | None:
        return (
            preferred_compiler(
                self.spack.compiler_info(cspec)
                for pkg in self.sp_spack.all_packages
                if (cspec := pkg.spec.root.compiler) is not None
            )
            if self.exists()
            else None
        )

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

        if self.recommended_compiler is None:
            comp = "(undefined)"
        elif not self.recommended_compiler:
            comp = f"{self.recommended_compiler} (unavailable)"
        else:
            comp = f"{self.recommended_compiler}"

        return f"""\
    Rec. Compiler        : {comp}
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
    def _specs(
        self,
    ) -> collections.abc.Mapping[str | None, collections.abc.Collection[sp_syntax.Spec]]:
        """Generate the set of Spack Specs that will need to be installed for the given config.

        The Specs are grouped by their `when:` clause.
        """
        result: dict[str | None, set[sp_syntax.Spec]] = collections.defaultdict(set)
        result[None] = {
            # NB: Meson is required as a build tool, but does not need to be included in the devenv
            # itself. It is instead installed separately in a venv by __main__.py.
            sp_syntax.Spec.parse("autoconf @2.69"),
            sp_syntax.Spec.parse("automake @1.15.1"),
            sp_syntax.Spec.parse("libtool @2.4.6"),
            sp_syntax.Spec.parse("m4"),
            sp_syntax.Spec.parse("gmake"),
            sp_syntax.Spec.parse(
                """boost @1.70.0:
                +atomic +date_time +filesystem +system +thread +timer +graph +regex
                +shared +multithreaded
                visibility=global"""
            ),
            sp_syntax.Spec.parse("bzip2 @1.0.8:"),
            sp_syntax.Spec.parse("dyninst @12.3.0:"),
            sp_syntax.Spec.parse("elfutils ~nls @0.186:"),
            sp_syntax.Spec.parse("intel-tbb +shared @2020.3"),
            sp_syntax.Spec.parse("libmonitor +hpctoolkit ~dlopen @2023.03.15:"),
            sp_syntax.Spec.parse("xz +pic libs=static @5.2.5:5.2.6,5.2.10:"),
            sp_syntax.Spec.parse("zlib +shared @1.2.13:1.2"),
            sp_syntax.Spec.parse("libunwind +xz +pic @1.6.2:"),
            sp_syntax.Spec.parse("libpfm4 @4.11.0:"),
            sp_syntax.Spec.parse("xerces-c transcoder=iconv @3.2.3:"),
            sp_syntax.Spec.parse("libiberty +pic @2.37:"),
            sp_syntax.Spec.parse("memkind"),
            sp_syntax.Spec.parse("yaml-cpp +shared @0.7.0:"),
            sp_syntax.Spec.parse("pkgconfig"),
            sp_syntax.Spec.parse("python +ctypes +lzma +bz2 +zlib @3.10.8:"),
        }
        result["arch.satisfies('target=x86_64:')"] = {
            sp_syntax.Spec.parse("intel-xed +pic @2022.04.17:")
        }

        if self.papi:
            result[None].add(sp_syntax.Spec.parse("papi @6.0.0.1:"))
        if self.cuda:
            result[None].add(sp_syntax.Spec.parse("cuda @10.2.89:"))
        if self.level0 or self.gtpin:
            result[None].add(sp_syntax.Spec.parse("oneapi-level-zero @1.7.15:"))
        if self.gtpin:
            result[None].add(sp_syntax.Spec.parse("intel-gtpin @3.0:"))
            result[None].add(sp_syntax.Spec.parse("oneapi-igc"))
        if self.opencl:
            result[None].add(sp_syntax.Spec.parse("opencl-c-headers @2022.01.04:"))
        if self.rocm:
            result[None].add(sp_syntax.Spec.parse("hip @5.1.3:"))
            result[None].add(sp_syntax.Spec.parse("hsa-rocr-dev @5.1.3:"))
            result[None].add(sp_syntax.Spec.parse("roctracer-dev @5.1.3:"))
            result[None].add(sp_syntax.Spec.parse("rocprofiler-dev @5.1.3:"))
        if self.mpi:
            result[None].add(sp_syntax.Spec.parse("mpi"))

        return result

    @classmethod
    def _yaml_merge(cls, dst: collections.abc.MutableMapping, src: collections.abc.Mapping):
        for k, v in src.items():
            if (
                isinstance(v, collections.abc.Mapping)
                and k in dst
                and isinstance(dst[k], collections.abc.MutableMapping)
            ):
                cls._yaml_merge(dst[k], v)
            else:
                dst[k] = v

    GITIGNORE: typing.ClassVar[str] = textwrap.dedent(
        """\
    # File created by the HPCToolkit ./dev script
    /**
    !/spack.yaml
    """
    )

    def generate(
        self,
        mode: DependencyMode,
        unresolve: collections.abc.Collection[str] = (),
        template: dict | None = None,
    ) -> None:
        """Generate the Spack environment."""
        yaml = ruamel.yaml.YAML(typ="safe")
        yaml.default_flow_style = False
        if mode == DependencyMode.LATEST:
            outerspack = spiqa.live_obj()

        self.root.mkdir(exist_ok=True)

        contents: dict[str, typing.Any] = {
            "spack": copy.deepcopy(template) if template is not None else {}
        }
        sp = contents.setdefault("spack", {})
        if "view" not in sp:
            sp["view"] = False

        sp.setdefault("concretizer", {})["unify"] = True
        sp["definitions"] = [d for d in contents["spack"].get("definitions", []) if "_dev" not in d]
        for when, subspecs in sorted(self._specs.items(), key=lambda kv: kv[0] or ""):
            clause_specs = []
            for spec in subspecs:
                match (DependencyMode.ANY if spec.root.package in unresolve else mode):
                    case DependencyMode.ANY:
                        clause_specs.append(spec)
                    case DependencyMode.MINIMUM:
                        clause_specs.append(spec.with_root(spec.root.with_minimized_versions))
                    case DependencyMode.LATEST:
                        clause_specs.append(
                            spec.with_root(outerspack.maximize_node_versions(spec.root))
                        )
            clause: dict[str, str | list[str]] = {
                "_dev": sorted([str(spec) for spec in clause_specs])
            }
            if when is not None:
                clause["when"] = when
            sp["definitions"].append(clause)

        if "$_dev" not in sp.setdefault("specs", []):
            sp["specs"].append("$_dev")

        with open(self.root / "spack.yaml", "w", encoding="utf-8") as f:
            yaml.dump(contents, f)

        with open(self.root / ".gitignore", "w", encoding="utf-8") as f:
            f.write(self.GITIGNORE)

        with open(self.root / "dev.json", "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f)

        (self.root / "spack.lock").unlink(missing_ok=True)

    def install(self) -> None:
        """Install the Spack environment."""
        try:
            self.sp_livespack.install(log_output=False)
        except subprocess.CalledProcessError as e:
            raise click.ClickException(f"spack install failed with code {e.returncode}") from e

    @functools.cached_property
    def bundle(self) -> spiqa.untrusted.ResolvedBundle:
        with (self.root / "spack.yaml").open("r", encoding="utf-8") as spackyamlf:
            env = ruamel.yaml.YAML(typ="safe").load(spackyamlf)

        files: dict[os.PathLike | str, bytes] = {}
        for path in self.root.rglob("*"):
            if path.parent == self.root and path.name == "spack.yaml":
                continue  # Handled above
            files[path.relative_to(self.root)] = path.read_bytes()

        return spiqa.untrusted.ResolvedBundle(env, user_files=files)

    def which(self, command: str, package: str, *, check_arg: str | None = "--version") -> Command:
        path = self.sp_spack.find(package).prefix
        if path is None:
            raise RuntimeError(f"No prefix for {package!r}")
        path /= "bin"
        cmd = shutil.which(command, path=path)
        if cmd is None:
            raise RuntimeError(f"Unable to find {command!r} in path {path!r}")

        result = Command(cmd)
        if check_arg is not None:
            result(check_arg, output=False)

        return result

    def _m_binaries(self, native: MesonMachineFile, *, compiler: Compiler | None = None) -> None:
        assert self.recommended_compiler is not None

        ccache_path = shutil.which("ccache")
        ccache = [Command(ccache_path)] if ccache_path else []

        if compiler is not None:
            if not compiler:
                raise ValueError(compiler)
            if not compiler.compatible_with(self.recommended_compiler):
                click.echo(
                    click.style("WARNING", fg="yellow")
                    + f": Requested compiler {compiler} may not be compatible with recommended compiler {self.recommended_compiler}, this may not work!"
                )
            native.add_binary("c", [*ccache, compiler.cc])
            native.add_binary("cpp", [*ccache, compiler.cpp])
        elif self.recommended_compiler:
            native.add_binary("c", [*ccache, self.recommended_compiler.cc])
            native.add_binary("cpp", [*ccache, self.recommended_compiler.cpp])
        else:
            click.echo(
                click.style("WARNING", fg="yellow")
                + f": Recommended compiler {self.recommended_compiler} is unavailable, Meson will use its own defaults instead!"
            )

        native.add_binary("autoreconf", self.which("autoreconf", "autoconf"))
        native.add_binary("autoconf", self.which("autoconf", "autoconf"))
        native.add_binary("aclocal", self.which("aclocal", "automake"))
        native.add_binary("autoheader", self.which("autoheader", "autoconf"))
        native.add_binary("autom4te", self.which("autom4te", "autoconf"))
        native.add_binary("automake", self.which("automake", "automake"))
        native.add_binary("libtoolize", self.which("libtoolize", "libtool"))
        native.add_binary("m4", self.which("m4", "m4"))
        native.add_binary("make", self.which("make", "gmake"))
        native.add_binary("python3", self.which("python3", "python"))
        native.add_binary("python", self.which("python3", "python"))
        if self.mpi:
            native.add_binary("mpicxx", self.which("mpicxx", "mpi", check_arg=None))
            native.add_binary("mpiexec", self.which("mpiexec", "mpi"))
        if self.cuda:
            native.add_binary("cuda", self.which("nvcc", "cuda", check_arg=None))
            native.add_binary("nvdisasm", self.which("nvdisasm", "cuda"))

    def _m_prefixes(self, native: MesonMachineFile) -> None:
        def prefix(pkg: str | spiqa.query.Package) -> Path:
            result = (self.sp_spack.find(pkg) if isinstance(pkg, str) else pkg).prefix
            if result is None:
                raise RuntimeError(f"No prefix for package: {pkg!r}")
            return result

        native.add_property("prefix_boost", prefix("boost"))
        native.add_property("prefix_bzip", prefix("bzip2"))
        native.add_property("prefix_dyninst", prefix("dyninst"))
        native.add_property("prefix_elfutils", prefix("elfutils"))
        native.add_property("prefix_tbb", prefix("intel-tbb"))
        native.add_property("prefix_libmonitor", prefix("libmonitor"))
        native.add_property("prefix_libunwind", prefix("libunwind"))
        native.add_property("prefix_perfmon", prefix("libpfm4"))
        native.add_property("prefix_xerces", prefix("xerces-c"))
        native.add_property("prefix_lzma", prefix("xz"))
        native.add_property("prefix_zlib", prefix("zlib"))
        native.add_property("prefix_libiberty", prefix("libiberty"))
        if xed_set := self.sp_spack.find_all("intel-xed"):
            native.add_property("prefix_xed", prefix(next(iter(xed_set))))
        native.add_property("prefix_memkind", prefix("memkind"))
        native.add_property("prefix_yaml_cpp", prefix("yaml-cpp"))
        if self.papi:
            native.add_property("prefix_papi", prefix("papi"))
        if self.opencl:
            native.add_property("prefix_opencl", prefix("opencl-c-headers"))
        if self.gtpin:
            native.add_property("prefix_gtpin", prefix("intel-gtpin"))
            native.add_property("prefix_igc", prefix("oneapi-igc"))
        if self.level0 or self.gtpin:
            native.add_property("prefix_level0", prefix("oneapi-level-zero"))
        if self.rocm:
            native.add_property("prefix_rocm_hip", prefix("hip"))
            native.add_property("prefix_rocm_hsa", prefix("hsa-rocr-dev"))
            native.add_property("prefix_rocm_tracer", prefix("roctracer-dev"))
            native.add_property("prefix_rocm_profiler", prefix("rocprofiler-dev"))
        if self.cuda:
            native.add_property("prefix_cuda", prefix("cuda"))

    def populate(self, *, compiler: Compiler | None = None) -> None:
        """Populate the development environment with all the files needed to use it."""
        native = MesonMachineFile()
        self._m_binaries(native, compiler=compiler)
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
