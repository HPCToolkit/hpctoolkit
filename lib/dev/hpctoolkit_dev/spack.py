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
    SPEC_VERSION_REGEX: typing.ClassVar = re.compile(r"@([A-Za-z0-9_.-:,]+)")
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
        if spack_env is not None:
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
    ) -> str:
        cmd: list[str | Path] = []
        if spack_env is not None:
            cmd.append(f"--env-dir={spack_env}")
        cmd.extend(args)
        return super().capture(
            *cmd,
            env=env,
            stdinput=stdinput,
            cwd=cwd,
        )

    def load(
        self,
        /,
        *names: str,
        spack_env: Path | None = None,
        env: collections.abc.Mapping[str, str] | None = None,
        cwd: Path | None = None,
    ) -> ShEnv:
        if len(names) > 1:
            text = f"Loading {len(names):d} packages"
        else:
            text = f"Loading {' '.join(names)}"
        with yaspin(text=text):
            return ShEnv(
                self.capture(
                    "load", "--only=package", "--sh", *names, spack_env=spack_env, env=env, cwd=cwd
                )
            )

    def get_prefix(
        self,
        spec: str,
        /,
        *,
        spack_env: Path | None = None,
        env: collections.abc.Mapping[str, str] | None = None,
        cwd: Path | None = None,
    ) -> Path:
        with yaspin(text=f"Locating dependency package ({spec})"):
            data = self.capture(
                "find", "--format={prefix}", spec, spack_env=spack_env, env=env, cwd=cwd
            )
            return Path(data.rstrip("\n")).resolve(strict=True)

    def install(
        self,
        /,
        *args: str | Path,
        spack_env: Path,
        env: collections.abc.Mapping[str, str] | None = None,
        cwd: Path | None = None,
    ) -> None:
        cmd: list[str | Path] = [self.path]
        if spack_env is not None:
            cmd.append(f"--env-dir={spack_env}")
        cmd.append("install")
        cmd.extend(args)

        if spack_env is not None:
            text_prefix = f"Installing dependencies in /{spack_env.name}"
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
