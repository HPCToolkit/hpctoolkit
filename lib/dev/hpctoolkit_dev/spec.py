import collections
import collections.abc
import contextlib
import copy
import enum
import functools
import json
import os
import re
import shutil
import subprocess
import textwrap
import typing
from pathlib import Path

import click
import ruamel.yaml

from .command import Command, ShEnv
from .spack import Spack, Version

__all__ = ("DependencyMode", "VerConSpec", "SpackEnv")


@enum.unique
class DependencyMode(enum.Enum):
    LATEST = "--latest"
    MINIMUM = "--minimum"
    ANY = "--any"


class VerConSpec:
    """Abstraction for a Spack spec with semi-customizable version constraints."""

    def __init__(self, spec: str, when: str | None = None) -> None:
        """Create a new VerConSpec based on the given Spack spec."""
        self._spec = Version.SPEC_VERSION_REGEX.sub("", spec)
        self._package = self.extract_package(spec)
        self._versions = Version.from_spec(spec)
        if self._versions:
            if not self._versions[0][0]:
                raise ValueError(f"Spec should specify a minimum version: {spec!r}")
            assert all(self._versions[0][0] <= vr[0] for vr in self._versions)
        self._when = when

    @staticmethod
    def extract_package(spec: str) -> str:
        package = re.match(r"^[a-zA-Z0-9_-]+", spec)
        if not package:
            raise ValueError(spec)
        return package[0]

    @property
    def package(self) -> str:
        return self._package

    @property
    def when(self) -> str | None:
        return self._when

    @functools.cached_property
    def version_spec(self) -> str:
        """Return the version constraints in Spack spec syntax."""
        if not self._versions:
            return "@:"
        return "@" + ",".join([f"{a or ''}:{b or ''}" for a, b in self._versions])

    def __str__(self) -> str:
        return f"{self._spec} {self.version_spec}" if self._versions else self._spec

    def version_ok(self, version: Version) -> bool:
        """Determine whether the given version is supported based on the constraints."""
        for bot, top in self._versions:
            if (not bot or bot <= version) and (not top or version <= top):
                return True
        return not self._versions

    @functools.cached_property
    def minimum_version(self) -> Version | None:
        """Determine the minimum supported version based on the constraints.
        Returns None if the version is unconstrained.
        """
        return self._versions[0][0] if self._versions else None

    @functools.cached_property
    def latest_version(self) -> Version | None:
        """Determine the latest available and supported version based on the constraints.
        Returns None if the version is unconstrained.
        """
        if not self._versions:
            return None
        latest = Spack.get().fetch_latest_ver(self._package)
        candidate: Version | None = None
        for bot, top in self._versions:
            if bot and latest < bot:
                # The latest from Spack is too old for this range. All ranges
                # after this point will have the same problem, so break.
                break
            if top and top < latest:
                # The latest from Spack is too new for this range, but top might
                # be the latest version supported by bot the range and Spack.
                # Save it as a possible candidate before rolling forward.
                candidate = top
                continue

            # The latest from Spack is valid. This will be the largest version
            # in the intersection, so use that.
            return latest

        # The latest from Spack doesn't satisfy the range, so take the latest
        # version in our range that Spack should be able to support.
        if candidate is None:
            raise RuntimeError(
                f"No supported latest version, intersection of @:{latest} and {self.version_spec} is empty"
            )
        return candidate

    @functools.cached_property
    def latest_external_version(self) -> Version | None:
        """Determine the latest available and supported version available as an external.
        Returns None if no such externals are available.
        """
        versions = Spack.get().fetch_external_versions(self._package)
        for bot, top in self._versions:
            if bot:
                versions = [v for v in versions if not v < bot]
            if top:
                versions = [v for v in versions if not top < v]
        return max(versions, default=None)

    def specs_for(self, mode: DependencyMode) -> set[str]:
        if not self._versions:
            return {self._spec}

        match mode:
            case DependencyMode.ANY:
                return {f"{self._spec} {self.version_spec}"}
            case DependencyMode.MINIMUM:
                return {f"{self._spec} @{self.minimum_version or ':'}"}
            case DependencyMode.LATEST:
                result = {f"{self._spec} {self.version_spec}"}

                vers: list[str] = []
                if v := self.latest_external_version:
                    vers.append(str(v))
                if v := self.latest_version:
                    vers.append(f"{v}:")
                if vers:
                    result.add(f"{self._spec} @{','.join(vers)}")

                return result

        raise AssertionError


def resolve_specs(
    specs: collections.abc.Iterable["VerConSpec"], mode: DependencyMode
) -> dict[str | None, set[str]]:
    by_when: dict[str | None, set[VerConSpec]] = {}
    for spec in specs:
        by_when.setdefault(spec.when, set()).add(spec)

    result: dict[str | None, set[str]] = {}
    for when, subspecs in by_when.items():
        bits: set[str] = set()
        for spec in subspecs:
            bits |= spec.specs_for(mode)
        result[when] = bits
    return result


class ParsedLockfile:
    """Cached data storage for the contents of a spack.lock file."""

    def __init__(self, root: Path) -> None:
        with open(root / "spack.lock", encoding="utf-8") as f:
            self._raw = json.load(f)

    @functools.cached_property
    def chashes(self) -> dict[str, str]:
        return {VerConSpec.extract_package(r["spec"]): r["hash"] for r in self._raw["roots"]}

    @functools.cached_property
    def versions(self) -> dict[str, Version]:
        return {k: Version(v["version"]) for k, v in self._raw["concrete_specs"].items()}

    @functools.cached_property
    def compilers(self) -> dict[str, tuple[str, Version]]:
        return {
            k: (v["compiler"]["name"], Version(v["compiler"]["version"]))
            for k, v in self._raw["concrete_specs"].items()
        }

    @functools.cached_property
    def specgraph(self) -> dict[str, list[dict]]:
        return {k: v.get("dependencies", []) for k, v in self._raw["concrete_specs"].items()}


if os.environ.get("CI") == "true":
    UPDATE_MSG = "\n  In CI: the reused merge-base image is out-of-date. Try updating the branch."
else:
    UPDATE_MSG = ""


class SpackEnv:
    """Abstraction for a single managed Spack environment."""

    def __init__(self, root: Path) -> None:
        self.root = root.resolve()
        if self.root.exists() and not self.root.is_dir():
            raise FileExistsError(self.root)

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

    @functools.cached_property
    def _lock(self) -> ParsedLockfile | None:
        return ParsedLockfile(self.root) if self.exists() else None

    def has_package(self, pkg: str) -> bool:
        assert self._lock
        return pkg in self._lock.chashes

    COMPILER_PREF = ("gcc", "clang")

    @property
    def recommended_compilerspec(self) -> str:
        if not self._lock:
            return "cc"
        comp, ver = max(
            (self._lock.compilers[chash] for _, chash in self._lock.chashes.items()),
            key=lambda c: (
                self.COMPILER_PREF.index(c[0]) if c[0] in self.COMPILER_PREF else -1,
                c[1],
            ),
        )
        return f"{comp}@{ver}"

    @property
    def recommended_compilers(self) -> tuple[Path, Path]:
        return Spack.get().fetch_compiler_paths(self.recommended_compilerspec)

    def load(self, *specs: VerConSpec, allow_missing_when: bool = False) -> ShEnv:
        assert self._lock
        hashes: set[str] = set()
        q = []

        for s in specs:
            h = self._lock.chashes.get(s.package)
            if h is None:
                if s.when and allow_missing_when:
                    continue
                raise click.ClickException(
                    f"Environment /{self.root.name} is missing concretization for expected package: {s.package}{UPDATE_MSG}"
                )
            if not s.version_ok(self._lock.versions[h]):
                raise click.ClickException(
                    f"Environment /{self.root.name} contains bad concretization for spec: {s}{UPDATE_MSG}"
                )
            q.append(h)

        while q:
            nexthash = q.pop()
            if nexthash not in hashes:
                hashes.add(nexthash)
                for dep in self._lock.specgraph[nexthash]:
                    if any(x in dep["type"] for x in ("run", "link")):
                        q.append(dep["hash"])
        return Spack.get().load(*["/" + h for h in hashes])

    def load_all(self) -> ShEnv:
        assert self._lock
        return self.load(*[VerConSpec(p) for p in self._lock.chashes])

    def prefix(self, pkg: str) -> Path:
        assert self._lock
        return Spack().get().get_prefix(f"{pkg}/{self._lock.chashes[pkg]}", spack_env=self.root)

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

    def generate_explicit(
        self,
        specs: collections.abc.Collection[VerConSpec],
        mode: DependencyMode,
        template: dict | None = None,
    ) -> None:
        """Generate the Spack environment."""
        yaml = ruamel.yaml.YAML(typ="safe")
        yaml.default_flow_style = False

        self.root.mkdir(exist_ok=True)

        contents: dict[str, typing.Any] = {
            "spack": copy.deepcopy(template) if template is not None else {}
        }
        sp = contents.setdefault("spack", {})
        if "view" not in sp:
            sp["view"] = False

        sp.setdefault("concretizer", {})["unify"] = True
        sp["definitions"] = [d for d in contents["spack"].get("definitions", []) if "_dev" not in d]
        for when, subspecs in sorted(
            resolve_specs(specs, mode).items(), key=lambda kv: kv[0] or ""
        ):
            clause: dict[str, str | list[str]] = {"_dev": sorted(subspecs)}
            if when is not None:
                clause["when"] = when
            sp["definitions"].append(clause)

        if "$_dev" not in sp.setdefault("specs", []):
            sp["specs"].append("$_dev")

        with open(self.root / "spack.yaml", "w", encoding="utf-8") as f:
            yaml.dump(contents, f)

        with open(self.root / ".gitignore", "w", encoding="utf-8") as f:
            f.write(self.GITIGNORE)

        (self.root / "spack.lock").unlink(missing_ok=True)
        with contextlib.suppress(AttributeError):
            del self._lock

    def install(self) -> None:
        """Install the Spack environment."""
        try:
            Spack.get().install("--fail-fast", spack_env=self.root)
        except subprocess.CalledProcessError as e:
            raise click.ClickException(f"spack install failed with code {e.returncode}") from e

    def which(
        self, command: str, spec: VerConSpec | None = None, *, check_arg: str | None = "--version"
    ) -> Command:
        path = (self.load(spec) if spec is not None else self.load_all())["PATH"]
        cmd = shutil.which(command, path=path)
        if cmd is None:
            raise RuntimeError(f"Unable to find {command} in path {path}")
        if cmd == shutil.which(command):
            raise RuntimeError(f"Command {command} was not provided by {spec!r}")

        result = Command(cmd)
        if check_arg is not None:
            result(check_arg, output=False)

        return result
