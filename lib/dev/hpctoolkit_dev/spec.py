import collections
import collections.abc
import contextlib
import enum
import functools
import json
import os
import re
import shutil
import stat
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
    def _lock_data(self) -> tuple[dict[str, str], dict[str, list[dict]]]:
        with open(self.root / "spack.lock", encoding="utf-8") as f:
            data = json.load(f)
        chashes: dict[str, str] = {
            VerConSpec.extract_package(r["spec"]): r["hash"] for r in data["roots"]
        }
        specgraph: dict[str, list[dict]] = {
            k: v.get("dependencies", []) for k, v in data["concrete_specs"].items()
        }
        return chashes, specgraph

    @property
    def _chashes(self) -> dict[str, str]:
        return self._lock_data[0]

    @property
    def _specgraph(self) -> dict[str, list[dict]]:
        return self._lock_data[1]

    def has_package(self, pkg: str) -> bool:
        return pkg in self._chashes

    def load(self, *pkgs: str) -> ShEnv:
        hashes: set[str] = set()
        q = [self._chashes[p] for p in pkgs]
        while q:
            nexthash = q.pop()
            if nexthash not in hashes:
                hashes.add(nexthash)
                for dep in self._specgraph[nexthash]:
                    if any(x in dep["type"] for x in ("run", "link")):
                        q.append(dep["hash"])
        return Spack.get().load(*["/" + h for h in hashes])

    def load_all(self) -> ShEnv:
        return self.load(*self._chashes.keys())

    def prefix(self, pkg: str) -> Path:
        return Spack().get().get_prefix(f"{pkg}/{self._chashes[pkg]}", spack_env=self.root)

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
        self, specs: collections.abc.Collection[VerConSpec], mode: DependencyMode
    ) -> None:
        """Generate the Spack environment."""
        yaml = ruamel.yaml.YAML(typ="safe")
        yaml.default_flow_style = False

        self.root.mkdir(exist_ok=True)

        contents: dict[str, typing.Any] = {"spack": {"view": False}}
        if (self.root / "spack.yaml").exists():
            with open(self.root / "spack.yaml", encoding="utf-8") as f:
                contents = yaml.load(f)

        contents["spack"].setdefault("concretizer", {})["unify"] = True

        contents["spack"]["definitions"] = [
            d for d in contents["spack"].get("definitions", []) if "_dev" not in d
        ]
        for when, subspecs in sorted(
            resolve_specs(specs, mode).items(), key=lambda kv: kv[0] or ""
        ):
            clause: dict[str, str | list[str]] = {"_dev": sorted(subspecs)}
            if when is not None:
                clause["when"] = when
            contents["spack"]["definitions"].append(clause)

        if "$_dev" not in contents["spack"].setdefault("specs", []):
            contents["spack"]["specs"].append("$_dev")

        with open(self.root / "spack.yaml", "w", encoding="utf-8") as f:
            yaml.dump(contents, f)

        with open(self.root / ".gitignore", "w", encoding="utf-8") as f:
            f.write(self.GITIGNORE)

        (self.root / "spack.lock").unlink(missing_ok=True)
        with contextlib.suppress(AttributeError):
            del self._lock_data

    def install(self) -> None:
        """Install the Spack environment."""
        try:
            Spack.get().install("--fail-fast", spack_env=self.root)
        except subprocess.CalledProcessError as e:
            raise click.ClickException(f"spack install failed with code {e.returncode}") from e

    def which(
        self, command: str, pkg: str | None = None, *, check_arg: str | None = "--version"
    ) -> Command:
        path = (self.load(pkg) if pkg is not None else self.load_all())["PATH"]
        cmd = shutil.which(command, path=path)
        if cmd is None:
            raise RuntimeError(f"Unable to find {command} in path {path}")
        if cmd == shutil.which(command):
            raise RuntimeError(f"Command {command} was not provided by package {pkg!r}")

        result = Command(cmd)
        if check_arg is not None:
            result(check_arg, output=False)

        return result

    def wrap(
        self, command: str, main_pkg: str, *pkgs: str, check_arg: str | None = "--version"
    ) -> Command:
        shenv = self.load(main_pkg, *pkgs)
        path = shenv["PATH"]
        cmd = shutil.which(command, path=path)
        if cmd is None:
            raise RuntimeError(f"Unable to find {command} in path {path}")
        if cmd == shutil.which(command):
            raise RuntimeError(
                f"Command {command} was not provided by package(s) {[main_pkg, *pkgs]}"
            )

        output = self.root / f"{main_pkg}-{command}"
        with open(output, "w", encoding="utf-8") as f:
            f.write("#!/bin/sh\n")
            f.write(re.sub(r"export (PYTHONPATH)=", r'export \1="$\1":', shenv.raw))
            f.write("\n")
            f.write(f'exec {cmd} "$@"\n')
        output.chmod(output.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

        result = Command(output)
        if check_arg is not None:
            result(check_arg, output=False)

        return result
