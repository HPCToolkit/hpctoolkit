import collections
import collections.abc
import enum
import functools
import re

from .spack import Version
from .spack import proxy as spack_proxy

__all__ = ("DependencyMode", "VerConSpec")


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
        latest = spack_proxy.fetch_latest_ver(self._package)
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
        versions = spack_proxy.fetch_external_versions(self._package)
        for bot, top in self._versions:
            if bot:
                versions = [v for v in versions if not v < bot]
            if top:
                versions = [v for v in versions if not top < v]
        return max(versions, default=None)

    def specs_for(
        self,
        mode: DependencyMode,
        /,
        *,
        unresolve: collections.abc.Collection[str] = (),
    ) -> set[str]:
        if not self._versions:
            return {self._spec}

        if self.package in unresolve:
            mode = DependencyMode.ANY
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
    specs: collections.abc.Iterable["VerConSpec"],
    mode: DependencyMode,
    /,
    *,
    unresolve: collections.abc.Collection[str] = (),
) -> dict[str | None, set[str]]:
    by_when: dict[str | None, set[VerConSpec]] = {}
    for spec in specs:
        by_when.setdefault(spec.when, set()).add(spec)

    result: dict[str | None, set[str]] = {}
    for when, subspecs in by_when.items():
        bits: set[str] = set()
        for spec in subspecs:
            bits |= spec.specs_for(mode, unresolve=unresolve)
        result[when] = bits
    return result
