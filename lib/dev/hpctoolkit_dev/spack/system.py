import enum
import re
import shutil
import typing
from pathlib import Path

import click

from .abc import CompilerBase
from .util import Version


class OSClass(enum.Enum):
    DebianLike = "Debian/Ubuntu"
    RedHatLike = "Fedora/RHEL"
    SUSELeap = "OpenSUSE Leap"


@typing.final
class SystemCompiler(CompilerBase):
    """Compiler that represents a compiler installed on the system, referred to via a simple syntax."""

    CC_TO_CPP = {
        "gcc": "g++",
        "clang": "clang++",
    }

    def __init__(self, basename: str) -> None:
        """Create a new SystemCompiler from the basename of the compiler."""
        match = re.fullmatch(r"([^=]+?)(?:-(\d+))?(?:=.*)?", basename)
        if not match:
            raise ValueError(f"Invalid syntax for compiler basename: {basename}")

        self._name, suffix = match[1], ("-" + match[2] if match[2] else "")
        if self._name not in self.CC_TO_CPP:
            raise ValueError(f"Unrecognized compiler name: {self._name!r}")

        self._cc = shutil.which(self._name + suffix)
        self._cpp = shutil.which(self.CC_TO_CPP[self._name] + suffix)

        self._basename = basename
        self._raw_version = int(match[2]) if match[2] else None
        self._version = Version(match[2]) if match[2] else Version("99")

    def os_packages(self, os: OSClass) -> set[str]:
        rv = self._raw_version
        match os:
            case OSClass.DebianLike:
                match self.name:
                    case "gcc":
                        return {f"gcc-{rv:d}", f"g++-{rv:d}"} if rv is not None else {"gcc", "g++"}
                    case "clang":
                        return (
                            {f"clang-{rv:d}", f"clang++-{rv:d}"}
                            if rv is not None
                            else {"clang", "clang++"}
                        )

            case OSClass.SUSELeap:
                match self.name:
                    case "gcc":
                        return (
                            {f"gcc{rv:d}", f"gcc{rv:d}-c++"}
                            if rv is not None
                            else {"gcc", "gcc-c++"}
                        )
                    case "clang":
                        return {f"clang{rv:d}"} if rv is not None else {"clang"}

            case OSClass.RedHatLike:
                if rv is not None:
                    raise click.ClickException(f"{os} does not support versioned compilers: {self}")

                match self.name:
                    case "gcc":
                        return {"gcc", "gcc-c++"}
                    case "clang":
                        return {"clang"}

        raise click.ClickException(f"Unsupported compiler for {os}: {self.name}")

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> Version:
        return self._version

    def __str__(self) -> str:
        return self._basename

    def __bool__(self) -> bool:
        return bool(self._cc and self._cpp)

    @property
    def cc(self) -> Path:
        if not self._cc:
            raise AttributeError("cc")
        return Path(self._cc)

    @property
    def cpp(self) -> Path:
        if not self._cpp:
            raise AttributeError("cpp")
        return Path(self._cpp)
