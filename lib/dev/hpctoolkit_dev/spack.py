import collections
import collections.abc
import functools
import re
import shutil
import subprocess
import typing
from pathlib import Path

import click
import ruamel.yaml
from yaspin import yaspin  # type: ignore[import]

from .command import Command, ShEnv

__all__ = ("Spack", "Version")

yaml = ruamel.yaml.YAML(typ="safe")


class Version:
    """Single version for a package, as understood by Spack.

    Ordered comparisons are weakly ordered based on Spack's understanding of
    satisfaction: x <= y only if x satisfies @:y or one is a prefix of the other.
    """

    VERSION_REGEX: typing.ClassVar = re.compile(r"[A-Za-z0-9_.-][A-Za-z0-9_.-]*")
    SEGMENT_REGEX: typing.ClassVar = re.compile(r"(?:([0-9]+)|([a-zA-Z]+))([_.-]*)")
    SPEC_VERSION_REGEX: typing.ClassVar = re.compile(r"@=?([A-Za-z0-9_.-:,]+)")
    INFINITY_SEGMENTS: typing.ClassVar = ("stable", "trunk", "head", "master", "main", "develop")

    __slots__ = ["_original", "_parsed"]
    _original: str
    _parsed: tuple[int | str, ...]

    def __init__(self, version: str) -> None:
        """Create a new Version from the specified version number."""
        if not self.VERSION_REGEX.fullmatch(version):
            raise ValueError(version)
        self._original = version
        self._parsed = tuple(
            alpha or int(num) for num, alpha, _ in self.SEGMENT_REGEX.findall(version)
        )

    @classmethod
    def from_spec(
        cls, spec: str
    ) -> list[tuple[typing.Optional["Version"], typing.Optional["Version"]]]:
        """Parse a full Spack spec for its version constraints, as pairs of Versions."""
        result = []
        for con in cls.SPEC_VERSION_REGEX.findall(spec):
            for rang in con.split(","):
                vers = rang.split(":")
                if len(vers) > 2:
                    raise ValueError(f"Invalid Spack version range: {rang!r}")
                if len(vers) == 2:
                    a = cls(vers[0]) if vers[0] else None
                    b = cls(vers[1]) if vers[1] else None
                    if a and b and a > b:
                        raise ValueError(f"Invalid Spack version range, {a} > {b}: {rang!r}")
                    result.append((a, b))
                else:
                    ver = cls(vers[0]) if vers[0] else None
                    result.append((ver, ver))
        return sorted(result)

    def __str__(self) -> str:
        return self._original

    @classmethod
    def _seg_le(cls, a: int | str, b: int | str) -> bool:
        """Determine if a <= b as version segments."""
        if isinstance(a, int):
            # If a and b are ints, compare directly.
            # If b is a str, a < b if b is an infinity and b < a otherwise.
            return a <= b if isinstance(b, int) else b in cls.INFINITY_SEGMENTS

        if a in cls.INFINITY_SEGMENTS:
            # If a and b are both infinities, compare by infinity "degree."
            # If b is not an infinity, b < a.
            return (
                cls.INFINITY_SEGMENTS.index(a) <= cls.INFINITY_SEGMENTS.index(b)
                if b in cls.INFINITY_SEGMENTS
                else False
            )

        # If a and b are both strings, compare lexigraphically.
        # If b is an infinity or is an int, a < b.
        return isinstance(b, int) or b in cls.INFINITY_SEGMENTS or a <= b

    def __le__(self, other: typing.Any) -> bool:
        if not isinstance(other, Version):
            return NotImplemented

        # If the parsed forms differ in a segment, that segment determines the comparison
        for pa, pb in zip(self._parsed, other._parsed, strict=False):
            if pa != pb:
                return self._seg_le(pa, pb)
        # One is a prefix of the other, they compare equivalent.
        return True

    def __ge__(self, other: typing.Any) -> bool:
        if not isinstance(other, Version):
            return NotImplemented

        # If the parsed forms differ in a segment, that segment determines the comparison
        for pa, pb in zip(self._parsed, other._parsed, strict=False):
            if pa != pb:
                return not self._seg_le(pa, pb)
        # One is a prefix of the other, they compare equivalent.
        return True

    def __lt__(self, other: typing.Any) -> bool:
        return not self.__ge__(other)

    def __gt__(self, other: typing.Any) -> bool:
        return not self.__le__(other)

    def __eq__(self, other: typing.Any) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._parsed == other._parsed

    def __ne__(self, other: typing.Any) -> bool:
        return not self.__eq__(other)


@typing.final
class Compiler:
    """Lazily-filled object representing a Compiler as Spack understands it."""

    @classmethod
    @functools.lru_cache
    def create(cls, spack: "Spack", name: str, version: str, spack_env: Path | None) -> "Compiler":
        return cls(spack, name, Version(version.removeprefix("=")), spack_env)

    def __init__(self, spack: "Spack", name: str, version: Version, spack_env: Path | None) -> None:
        self._spack = spack
        self._spack_env = spack_env
        self._name = name
        self._version = version

    def __str__(self) -> str:
        return f"%{self.spec}"

    @property
    def spec(self) -> str:
        """Spack spec for the compiler represented here."""
        return f"{self._name}@={self._version}"

    COMPILER_PREFERENCE_ORDER = ("gcc", "clang")

    def preferred_over(self, other: "Compiler") -> bool:
        """Determine if this Compiler is preferred for practical use over `other`."""
        if self._name != other._name:
            if self._name in self.COMPILER_PREFERENCE_ORDER:
                if other._name in self.COMPILER_PREFERENCE_ORDER:
                    return self.COMPILER_PREFERENCE_ORDER.index(
                        self._name
                    ) < self.COMPILER_PREFERENCE_ORDER.index(other._name)
                return True
            if other._name in self.COMPILER_PREFERENCE_ORDER:
                return False
            # We don't know either compiler, and they differ. Make an arbitrary decision.
            return self._name < other._name
        if self._version != other._version:
            return self._version >= other._version
        # At this point they seem equal. Either option is fine, return something.
        return False

    @staticmethod
    def preferred(compilers: collections.abc.Iterable["Compiler"]) -> "Compiler":
        """Fetch the "most preferred" compiler from the set."""
        result: Compiler | None = None
        for c in compilers:
            if result is None or c.preferred_over(result):
                result = c
        if result is None:
            raise ValueError("Expected at least one Compiler, got none")
        return result

    @functools.cached_property
    def _info(self) -> tuple[str, str]:
        with yaspin(text=f"Fetching info on compiler {self.spec}"):
            data = self._spack.capture("compiler", "info", self.spec, spack_env=self._spack_env)

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

    @functools.cached_property
    def cc(self) -> Path:
        """C compiler used by Spack with this Compiler."""
        return Path(self._info[0]).resolve(strict=True)

    @functools.cached_property
    def cpp(self) -> Path:
        """C++ compiler used by Spack with this Compiler."""
        return Path(self._info[1]).resolve(strict=True)


@typing.final
class Package:
    """Lazily-filled object representing a package installed by Spack. Internally identified by the
    full hash of the package.
    """

    @classmethod
    @functools.lru_cache
    def create(cls, spack: "Spack", encoded: str, spack_env: Path | None) -> "Package":
        return cls(spack, encoded, spack_env)

    ENCODED_DELIMITER = "|NEXT|"
    ENCODED_FORMAT = (
        "({hash}({name}@={version} {/hash:7}({name}({compiler.name}@{compiler.version})"
        + ENCODED_DELIMITER
    )
    ENCODED_REGEX = re.compile(
        r"\((?P<fullhash>[^(]+)\((?P<pretty>[^(]+)\((?P<package>[^(]+)\((?P<compiler>[^)]+)\)"
    )
    COMPILER_REGEX = re.compile(r"([a-zA-Z0-9_][a-zA-Z0-9_-]*)@(=?[a-zA-Z0-9_][a-zA-Z_0-9.-]*)")

    def __init__(self, spack: "Spack", encoded: str, spack_env: Path | None) -> None:
        self._spack = spack
        self._spack_env = spack_env
        if not (m := self.ENCODED_REGEX.fullmatch(encoded)):
            raise ValueError(f"Invalid encoded package: {encoded!r}")
        self._fullhash, self._pretty, self._package = m["fullhash"], m["pretty"], m["package"]
        if not (mc := self.COMPILER_REGEX.fullmatch(m["compiler"])):
            raise RuntimeError(f"Unable to parse Spack compiler: {m['compiler']}")
        self._compiler = Compiler.create(self._spack, mc[1], mc[2], self._spack_env)

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
    def compiler(self) -> Compiler:
        """Spack identifier for the compiler used to build this Package."""
        return self._compiler

    @functools.cached_property
    def prefix(self) -> Path:
        """Installation prefix where the Package is installed."""
        with yaspin(text=f"Locating package {self.pretty}"):
            data = self._spack.capture(
                "find", "--format={prefix}", "--no-groups", self.spec, spack_env=self._spack_env
            )
        return Path(data.rstrip("\n")).resolve(strict=True)

    @functools.cached_property
    def dependencies(self) -> frozenset["Package"]:
        """Set of all (transitive and run-type) dependencies of this Package."""
        with yaspin(text=f"Fetching dependencies of {self.pretty}"):
            data = self._spack.capture(
                "dependencies",
                "--installed",
                "--deptype=run",
                "--transitive",
                self.spec,
                spack_env=self._spack_env,
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
            if (
                pkg := self._spack.fetch(
                    f"{package}@{version} /{shorthash}", spack_env=self._spack_env
                )
            ) is not None:
                result.add(pkg)
        return frozenset(result)

    @functools.cached_property
    def load(self) -> ShEnv:
        """Generate the environment produces when this package gets loaded."""
        with yaspin(text=f"Loading package {self.pretty}"):
            return ShEnv(
                self._spack.capture(
                    "load", "--only=package", "--sh", self.spec, spack_env=self._spack_env
                )
            )


@typing.final
class Spack(Command):
    """Command that is designed to represent Spack."""

    def __init__(self) -> None:
        spack = shutil.which("spack")
        if not spack:
            raise click.ClickException("Spack was not found on the PATH.")
        super().__init__(spack)

    _singleton: typing.ClassVar["Spack"]

    @classmethod
    def get(cls) -> "Spack":
        if not hasattr(cls, "_singleton"):
            cls._singleton = cls()
        return cls._singleton

    def __call__(
        self,
        /,
        *args: str | Path,
        spack_env: Path | None = None,
        env: collections.abc.Mapping[str, str] | None = None,
        stdinput: str | None = None,
        output: bool = True,
        cwd: Path | None = None,
    ) -> None:
        cmd: list[str | Path] = []
        if spack_env is None:
            cmd.append("--no-env")
        else:
            cmd.append(f"--env-dir={spack_env}")
        cmd.extend(args)
        return super().__call__(
            *cmd,
            env=env,
            stdinput=stdinput,
            output=output,
            cwd=cwd,
        )

    def capture(
        self,
        /,
        *args: str | Path,
        spack_env: Path | None = None,
        env: collections.abc.Mapping[str, str] | None = None,
        stdinput: str | None = None,
        cwd: Path | None = None,
        error_output: bool = True,
    ) -> str:
        cmd: list[str | Path] = []
        if spack_env is None:
            cmd.append("--no-env")
        else:
            cmd.append(f"--env-dir={spack_env}")
        cmd.extend(args)
        return super().capture(
            *cmd,
            env=env,
            stdinput=stdinput,
            cwd=cwd,
            error_output=error_output,
        )

    def fetch_all(
        self,
        spec: str | None = None,
        /,
        *,
        spack_env: Path | None = None,
        concretized: bool = False,
    ) -> set[Package]:
        cmd = ["find", "--format=" + Package.ENCODED_FORMAT, "--no-groups"]
        if concretized:
            cmd.append("--show-concretized")
        if spec is None:
            if spack_env is not None:
                msg = f"Listing all packages in environment /{spack_env.name}"
            else:
                msg = "Listing all packages"
        else:
            cmd.append(spec)
            msg = f"Listing packages matching {spec}"

        with yaspin(text=msg):
            try:
                data = self.capture(*cmd, spack_env=spack_env, error_output=False)
            except subprocess.CalledProcessError:
                data = ""
        return {
            Package.create(self, se, spack_env)
            for e in data.split(Package.ENCODED_DELIMITER)
            if (se := e.strip())
        }

    def fetch(
        self, spec: str, /, *, spack_env: Path | None = None, concretized: bool = False
    ) -> Package:
        cmd = ["find", "--format=" + Package.ENCODED_FORMAT, "--no-groups"]
        if concretized:
            cmd.append("--show-concretized")
        cmd.append(spec)
        msg = f"Finding package {spec}"

        with yaspin(text=msg):
            try:
                data = self.capture(*cmd, spack_env=spack_env, error_output=False)
            except subprocess.CalledProcessError:
                raise RuntimeError(f"Missing expected package {spec!r}") from None
        encodings = [se for e in data.split(Package.ENCODED_DELIMITER) if (se := e.strip())]
        if len(encodings) > 1:
            raise RuntimeError(f"Multiple packages match spec {spec!r}")
        return Package.create(self, encodings[0], spack_env)

    def view_add(
        self,
        /,
        target: Path,
        *packages: Package,
        spack_env: Path | None = None,
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
                spack_env=spack_env,
                env=env,
                cwd=cwd,
                output=False,
            )

    def install(
        self,
        /,
        *args: str | Path,
        spack_env: Path,
        env: collections.abc.Mapping[str, str] | None = None,
        cwd: Path | None = None,
    ) -> None:
        cmd: list[str | Path] = [self.path]
        if spack_env is None:
            cmd.append("--no-env")
        else:
            cmd.append(f"--env-dir={spack_env}")
        cmd.append("install")
        cmd.extend(args)

        if spack_env is not None:
            text_prefix = f"Installing dependencies in .../{spack_env.name}"
        else:
            text_prefix = "Installing dependencies"
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

    def config_get(self, section: str) -> dict:
        """Fetch a section of the active Spack config."""
        with yaspin(text=f"Fetching configuration ({section}.yaml)"):
            raw_data = self.capture("--no-env", "config", "get", section)
            return yaml.load(raw_data)

    @functools.cached_property
    def packages_yaml(self) -> dict:
        return self.config_get("packages")

    @functools.cached_property
    def compilers_yaml(self) -> dict:
        return self.config_get("compilers")

    def fetch_external_versions(self, pkg: str) -> list[Version]:
        """Probe Spack for the versions of a package supplied as externals."""
        return [
            Version(ver[1])
            for ext in self.packages_yaml.get("packages", {}).get(pkg, {}).get("externals", [])
            if (ver := Version.SPEC_VERSION_REGEX.search(ext.get("spec", "")))
        ]

    def fetch_latest_ver(self, pkg: str) -> Version:
        """Probe Spack for the latest available version of the given package."""
        with yaspin(text=f"Fetching versions of {pkg}"):
            data = self.capture("versions", "--safe", pkg)
            for ver in data.split("\n"):
                ver = ver.strip()
                if re.search(r"\b(develop|main|master|head|trunk|stable)\b", ver):
                    # Infinity version, ignore
                    continue
                return Version(ver)
            raise ValueError(f'Unable to find Spack "Safe versions" for package {pkg}')
