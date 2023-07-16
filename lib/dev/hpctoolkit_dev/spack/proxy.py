import collections
import collections.abc
import functools
import re
import shutil
import subprocess
import tempfile
import typing
from pathlib import Path

import click
import ruamel.yaml
from yaspin import yaspin  # type: ignore[import]

from ..command import Command, ShEnv
from .abc import CompilerBase, PackageBase, SpackBase
from .util import Version

yaml = ruamel.yaml.YAML(typ="safe")


@typing.final
class ProxyCompiler(CompilerBase):
    """Lazily-filled object representing a Compiler as Spack understands it."""

    @classmethod
    @functools.lru_cache
    def create(cls, spack: "ProxySpack", name: str, version: str) -> "ProxyCompiler":
        return cls(spack, name, Version(version.removeprefix("=")))

    def __init__(self, spack: "ProxySpack", name: str, version: Version) -> None:
        self._spack = spack
        self._name = name
        self._version = version

    def __str__(self) -> str:
        return f"%{self.spec}"

    @property
    def spec(self) -> str:
        """Spack spec for the compiler represented here."""
        return f"{self._name}@={self._version}"

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> Version:
        return self._version

    @functools.cached_property
    def fake_compiler_yaml_dir(self) -> tempfile.TemporaryDirectory:
        """Workaround for a complication when Spack can't seem to operate without its compiler."""
        result = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        path = Path(result.name)
        with (path / "compilers.yaml").open("w", encoding="utf-8") as yamlf:
            ruamel.yaml.YAML(typ="safe").dump(
                {
                    "compilers:": [
                        {
                            "compiler": {
                                "spec": self.spec,
                                "operating_system": self._spack.operating_system,
                                "paths": {
                                    "cc": "/bin/false",
                                    "cxx": "/bin/false",
                                    "f77": "/bin/false",
                                    "fc": "/bin/false",
                                },
                                "modules": [],
                            }
                        }
                    ]
                },
                yamlf,
            )
        return result

    @functools.cached_property
    def _info(self) -> tuple[str, str] | None:
        try:
            with yaspin(text=f"Fetching info on compiler {self.spec}"):
                data = self._spack.capture("compiler", "info", self.spec, error_output=False)
        except subprocess.CalledProcessError:
            return None

        # FIXME: The output is vaugely machine-readable, but not in any standard format. Ugh.
        trials: list[dict[str, str]] = []
        for line in data.split("\n"):
            if not line:
                continue
            if not line[0].isspace():
                trials.append({})
            elif m := re.fullmatch(r"\s+cc\s+=\s+([/\\].+)", line):
                trials[-1]["cc"] = m[1]
            elif m := re.fullmatch(r"\s+cxx\s+=\s+([/\\].+)", line):
                trials[-1]["cxx"] = m[1]
        for t in trials:
            if "cc" in t and "cxx" in t:
                return (t["cc"], t["cxx"])
        raise RuntimeError(f"Spack compiler is incomplete or nonexistent: {self.spec}")

    def __bool__(self) -> bool:
        return self._info is not None

    @functools.cached_property
    def cc(self) -> Path:
        """C compiler used by Spack with this Compiler."""
        if (i := self._info) is None:
            raise AttributeError("cc")
        return Path(i[0]).resolve(strict=True)

    @functools.cached_property
    def cpp(self) -> Path:
        """C++ compiler used by Spack with this Compiler."""
        if (i := self._info) is None:
            raise AttributeError("cpp")
        return Path(i[1]).resolve(strict=True)


@typing.final
class ProxyPackage(PackageBase):
    """Lazily-filled object representing a package installed by Spack. Internally identified by the
    full hash of the package.
    """

    @classmethod
    @functools.lru_cache
    def create(cls, spack: "ProxySpack", encoded: str) -> "ProxyPackage":
        return cls(spack, encoded)

    ENCODED_DELIMITER = "|NEXT|"
    ENCODED_FORMAT = (
        "({hash}({name}@={version} {/hash:7}({name}({compiler.name}@{compiler.version})"
        + ENCODED_DELIMITER
    )
    ENCODED_REGEX = re.compile(
        r"\((?P<fullhash>[^(]+)\((?P<pretty>[^(]+)\((?P<package>[^(]+)\((?P<compiler>[^)]+)\)"
    )
    COMPILER_REGEX = re.compile(r"([a-zA-Z0-9_][a-zA-Z0-9_-]*)@(=?[a-zA-Z0-9_][a-zA-Z_0-9.-]*)")

    def __init__(self, spack: "ProxySpack", encoded: str) -> None:
        self._spack = spack
        if not (m := self.ENCODED_REGEX.fullmatch(encoded)):
            raise ValueError(f"Invalid encoded package: {encoded!r}")
        self._fullhash, self._pretty, self._package = m["fullhash"], m["pretty"], m["package"]
        if not (mc := self.COMPILER_REGEX.fullmatch(m["compiler"])):
            raise RuntimeError(f"Unable to parse Spack compiler: {m['compiler']}")
        self._compiler = ProxyCompiler.create(self._spack, mc[1], mc[2])

    def __str__(self) -> str:
        try:
            return self.__dict__["pretty"]
        except KeyError:
            return self.spec

    @property
    def fullhash(self) -> str:
        """Full hash identifying the installed Package."""
        return self._fullhash

    @property
    def spec(self) -> str:
        """Identifier for the Package as a Spack spec. Not human-readable, see pretty."""
        return f"/{self._fullhash}"

    @property
    def package(self) -> str:
        """Name of the Spack package that was installed to make this Package."""
        return self._package

    @property
    def pretty(self) -> str:
        """Pretty name for the installed Package. Looks nicer than the fullhash."""
        return self._pretty

    @property
    def compiler(self) -> ProxyCompiler:
        """Spack identifier for the compiler used to build this Package."""
        return self._compiler

    @functools.cached_property
    def prefix(self) -> Path:
        """Installation prefix where the Package is installed."""
        with yaspin(text=f"Locating package {self.pretty}"):
            data = self._spack.capture("find", "--format={prefix}", "--no-groups", self.spec)
        return Path(data.rstrip("\n")).resolve(strict=True)

    @functools.cached_property
    def dependencies(self) -> frozenset["ProxyPackage"]:
        """Set of all (transitive and run-type) dependencies of this Package."""
        with yaspin(text=f"Fetching dependencies of {self.pretty}"):
            data = self._spack.capture(
                "dependencies", "--installed", "--deptype=run", "--transitive", self.spec
            )
        # FIXME: Spack doesn't provide a way to control the format of specs output by this function,
        # so we need to parse the human-readable output instead. Ugh.
        result = set()
        for m in re.finditer(
            r"([a-zA-Z_0-9]+) ([a-zA-Z_0-9][a-zA-Z_0-9-]*)@(=?[a-zA-Z0-9_][a-zA-Z_0-9.-]*\b)", data
        ):
            shorthash, package, version = m[1], m[2], m[3]
            if not version.startswith("="):
                version = f"={version}"
            if (pkg := self._spack.fetch(f"{package}@{version} /{shorthash}")) is not None:
                result.add(pkg)
        return frozenset(result)

    @functools.cached_property
    def load(self) -> ShEnv:
        """Generate the environment produces when this package gets loaded."""
        try:
            with yaspin(text=f"Loading package {self.pretty}"):
                loadsh = self._spack.capture(
                    "load", "--only=package", "--sh", self.spec, error_output=False
                )
        except subprocess.CalledProcessError:
            fake_dir = self.compiler.fake_compiler_yaml_dir.name
            with yaspin(text=f"Loading package {self.pretty} (with fake compilers)"):
                loadsh = self._spack.capture(
                    "-C", fake_dir, "load", "--only=package", "--sh", self.spec
                )
        return ShEnv(loadsh)


def which_spack() -> Command:
    spack = shutil.which("spack")
    if not spack:
        raise click.ClickException("Spack was not found on the PATH.")
    return Command(spack)


@typing.final
class ProxySpack(SpackBase):
    """Interface to Spack that actually calls Spack in the back. Bound to a specific Spack environment."""

    def __init__(self, spack_env: Path) -> None:
        self.real = which_spack()
        self.spack_env = spack_env.resolve(strict=True)
        if not self.spack_env.is_dir() or not (self.spack_env / "spack.yaml").is_file():
            raise ValueError(f"Path is not a Spack environment: {self.spack_env}")

    def __call__(
        self,
        /,
        *args: str | Path,
        env: collections.abc.Mapping[str, str] | None = None,
        stdinput: str | None = None,
        output: bool = True,
        cwd: Path | None = None,
    ) -> None:
        return self.real(
            f"--env-dir={self.spack_env}",
            *args,
            env=env,
            stdinput=stdinput,
            output=output,
            cwd=cwd,
        )

    def capture(
        self,
        /,
        *args: str | Path,
        env: collections.abc.Mapping[str, str] | None = None,
        stdinput: str | None = None,
        cwd: Path | None = None,
        error_output: bool = True,
    ) -> str:
        return self.real.capture(
            f"--env-dir={self.spack_env}",
            *args,
            env=env,
            stdinput=stdinput,
            cwd=cwd,
            error_output=error_output,
        )

    @functools.cached_property
    def operating_system(self) -> str:
        """Operating system as Spack understands it."""
        with yaspin(text="Querying Spack architecture"):
            return self.capture("arch", "--operating-system").strip()

    def fetch_all(
        self,
        spec: str | None = None,
        /,
        *,
        concretized: bool = False,
    ) -> set[ProxyPackage]:
        cmd = ["find", "--format=" + ProxyPackage.ENCODED_FORMAT, "--no-groups"]
        if concretized:
            cmd.append("--show-concretized")
        if spec is None:
            msg = f"Listing all packages in environment {self.spack_env.name}/"
        else:
            cmd.append(spec)
            msg = f"Listing packages matching {spec}"

        with yaspin(text=msg):
            try:
                data = self.capture(*cmd, error_output=False)
            except subprocess.CalledProcessError:
                data = ""
        return {
            ProxyPackage.create(self, se)
            for e in data.split(ProxyPackage.ENCODED_DELIMITER)
            if (se := e.strip())
        }

    def fetch(self, spec: str, /, *, concretized: bool = False) -> ProxyPackage:
        cmd = ["find", "--format=" + ProxyPackage.ENCODED_FORMAT, "--no-groups"]
        if concretized:
            cmd.append("--show-concretized")
        cmd.append(spec)
        msg = f"Finding package {spec}"

        with yaspin(text=msg):
            try:
                data = self.capture(*cmd, error_output=False)
            except subprocess.CalledProcessError:
                raise RuntimeError(f"Missing expected package {spec!r}") from None
        encodings = [se for e in data.split(ProxyPackage.ENCODED_DELIMITER) if (se := e.strip())]
        if len(encodings) > 1:
            raise RuntimeError(f"Multiple packages match spec {spec!r}")
        return ProxyPackage.create(self, encodings[0])

    def view_add(
        self,
        /,
        target: Path,
        *packages: PackageBase,
        env: collections.abc.Mapping[str, str] | None = None,
        cwd: Path | None = None,
    ):
        if len(packages) > 1:
            text = f"Adding {len(packages):d} packages to view"
        else:
            text = f"Adding {', '.join([p.pretty for p in packages])} to view"
        with yaspin(text=text):
            self(
                "view",
                "--dependencies=no",
                "add",
                "--ignore-conflicts",
                target,
                *[p.spec for p in packages],
                env=env,
                cwd=cwd,
                output=False,
            )

    def install(
        self,
        /,
        *args: str | Path,
        env: collections.abc.Mapping[str, str] | None = None,
        cwd: Path | None = None,
    ) -> None:
        cmd: list[str | Path] = [self.real.path, f"--env-dir={self.spack_env}", "install", *args]
        text_prefix = f"Installing dependencies in {self.spack_env.name}/"
        with yaspin(text=text_prefix, timer=True).sand as sp:
            try:
                with subprocess.Popen(cmd, env=env, cwd=cwd, stdout=subprocess.PIPE) as proc:
                    assert proc.stdout is not None
                    for line in proc.stdout:
                        if re.match(rb"^\[.\]", line):
                            sp.text = text_prefix
                        elif m := re.search(rb"Installing (\S+)", line):
                            target = m[1].decode("utf-8")
                            sp.text = f"{text_prefix} ({target})"
                    sp.text = f"{text_prefix} (finalizing)"
            except BaseException:
                proc.kill()
                proc.wait()
                sp.fail()
                raise

            if proc.returncode != 0:
                sp.fail()
                raise subprocess.CalledProcessError(proc.returncode, cmd)

            sp.text = text_prefix
            sp.ok()


@functools.cache
def packages_yaml() -> dict:
    with yaspin(text="Fetching configuration (packages.yaml)"):
        return yaml.load(which_spack().capture("--no-env", "config", "get", "packages"))


@functools.cache
def fetch_external_versions(pkg: str) -> list[Version]:
    """Probe Spack for the versions of a package supplied as externals."""
    return [
        Version(ver[1])
        for ext in packages_yaml().get("packages", {}).get(pkg, {}).get("externals", [])
        if (ver := Version.SPEC_VERSION_REGEX.search(ext.get("speget_spackc", "")))
    ]


@functools.cache
def fetch_latest_ver(pkg: str) -> Version:
    """Probe Spack for the latest available version of the given package."""
    with yaspin(text=f"Fetching versions of {pkg}"):
        data = which_spack().capture("--no-env", "versions", "--safe", pkg)
        for raw_ver in data.split("\n"):
            ver = raw_ver.strip()
            if re.search(r"\b(develop|main|master|head|trunk|stable)\b", ver):
                # Infinity version, ignore
                continue
            return Version(ver)
        raise ValueError(f'Unable to find Spack "Safe versions" for package {pkg}')
