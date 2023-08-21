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
import spiqa
import spiqa.query
from spiqa.syntax import CompilerSpec, Version
from yaspin import yaspin  # type: ignore[import]

from ..command import Command
from .abc import CompilerBase, SpackBase

yaml = ruamel.yaml.YAML(typ="safe")


@typing.final
class ProxyCompiler(CompilerBase):
    """Lazily-filled object representing a Compiler as Spack understands it."""

    @classmethod
    @functools.lru_cache
    def create(cls, spack: "ProxySpack", spec: CompilerSpec) -> "ProxyCompiler":
        return cls(spack, spec)

    def __init__(self, spack: "ProxySpack", spec: CompilerSpec) -> None:
        self._spack = spack
        if not hasattr(spec.versions, "singleton"):
            raise ValueError(f"Must be a concrete compiler: {spec}")
        self._spec = spec

    def __str__(self) -> str:
        return self.spec

    @property
    def spec(self) -> str:
        """Spack spec for the compiler represented here."""
        return str(self._spec)

    @property
    def name(self) -> str:
        return self._spec.compiler

    @property
    def version(self) -> Version:
        return self._spec.versions.singleton

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
                                "operating_system": spiqa.live_obj().arch.os,
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
                data = self._spack.capture(
                    "compiler", "info", self.spec.removeprefix("%"), error_output=False
                )
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

    def compiler_info(self, /, compiler: CompilerSpec) -> ProxyCompiler:
        return ProxyCompiler.create(self, compiler)
