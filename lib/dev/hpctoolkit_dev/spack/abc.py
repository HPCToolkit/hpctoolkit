import abc
import typing
from pathlib import Path

from ..command import ShEnv
from .util import Version


class CompilerBase(abc.ABC):
    """Representation of a Compiler that can be used to configure the build directory."""

    @abc.abstractmethod
    def __str__(self) -> str:
        """Human-readable identifier for the Compiler. If registered with Spack, should be
        in Spack spec-syntax.
        """

    @property
    @abc.abstractmethod
    def name(self) -> str:
        """Simple identifier for the Compiler family, e.g. gcc or clang."""

    @property
    @abc.abstractmethod
    def version(self) -> Version:
        """Specific version of the Compiler in question."""

    @abc.abstractmethod
    def __bool__(self) -> bool:
        """Return whether the Compiler actually exists on the system or not."""

    @property
    @abc.abstractmethod
    def cc(self) -> Path:
        """Path to the C compiler represented by this object. If not self, raise AttributeError."""

    @property
    @abc.abstractmethod
    def cpp(self) -> Path:
        """Path to the C++ compiler represented by this object. If not self, raise AttributeError."""

    @typing.final
    def compatible_with(self, other: "CompilerBase") -> bool:
        """Determine if this Compiler can almost certainly link the output of an `other` Compiler.
        Returns False if it is unlikely or just unknown if this will work.
        """
        return self.name == other.name and self.version >= other.version

    COMPILER_PREFERENCE_ORDER: typing.Final = ("gcc", "clang")

    @typing.final
    def preferred_over(self, other: "CompilerBase") -> bool:
        """Determine if this Compiler is preferred for practical use over `other`."""
        if self.name != other.name:
            if self.name in self.COMPILER_PREFERENCE_ORDER:
                if other.name in self.COMPILER_PREFERENCE_ORDER:
                    return self.COMPILER_PREFERENCE_ORDER.index(
                        self.name
                    ) < self.COMPILER_PREFERENCE_ORDER.index(other.name)
                return True
            if other.name in self.COMPILER_PREFERENCE_ORDER:
                return False
            # We don't know either compiler, and they differ. Make an arbitrary decision.
            return self.name < other.name
        if self.version != other.version:
            return self.version >= other.version
        # At this point they seem equal. Either option is fine, return something.
        return False


class PackageBase(abc.ABC):
    """Descriptor object representing a Spack-installed Package."""

    @abc.abstractmethod
    def __str__(self) -> str:
        """Return a representation of the Package in Spack spec syntax. Preferably human-readable
        but do not trigger Spack calls.
        """

    @property
    @abc.abstractmethod
    def fullhash(self) -> str:
        """Full hash identifying the installed Package. Base32 string."""

    @property
    @abc.abstractmethod
    def spec(self) -> str:
        """Identifier for the unique Package as a Spack spec. Not human-readable, see pretty."""

    @property
    @abc.abstractmethod
    def package(self) -> str:
        """Name of the Spack package that was installed to make this Package."""

    @property
    @abc.abstractmethod
    def pretty(self) -> str:
        """Pretty name for the installed Package. Looks nicer than the fullhash."""

    @property
    @abc.abstractmethod
    def compiler(self) -> CompilerBase:
        """Spack identifier for the compiler used to build this Package."""

    @property
    @abc.abstractmethod
    def prefix(self) -> Path:
        """Installation prefix where the Package is installed."""

    @property
    @abc.abstractmethod
    def dependencies(self) -> frozenset["PackageBase"]:
        """Set of all (transitive and run-type) dependencies of this Package."""

    @property
    @abc.abstractmethod
    def load(self) -> ShEnv:
        """Generate the environment produces when this package gets loaded."""


class SpackBase(abc.ABC):
    """Generic interface to access Spack-built packages. Bound to a specific Spack environment."""

    @abc.abstractmethod
    def __init__(self, spack_env: Path) -> None:
        """Create a new Spack instance bound to the given Spack environment."""
