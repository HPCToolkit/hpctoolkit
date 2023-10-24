import collections.abc
from pathlib import Path

from .command import Command


class MesonMachineFile:
    """Formatter for Meson native/cross files. The format is a little like INI or TOML but the
    values are a restricted set of the Meson language (i.e. Meson without function calls).
    The interface provided by this object exposes the features roughly as a dict of strs.
    """

    def __init__(self) -> None:
        self._binaries: dict[str, str] = {}
        self._properties: dict[str, str] = {}
        self._project_options: dict[str, str] = {}
        self._builtin_options: dict[str, str] = {}

    def _norm(self, value: str | Path | int | collections.abc.Iterable[str | Path | int]) -> str:
        if isinstance(value, int):
            return f"{value:d}"
        if isinstance(value, str | Path):
            return "'{}'".format(str(value).replace("'", "\\'"))
        if isinstance(value, collections.abc.Iterable):
            return "\n  ".join(["["] + [self._norm(v) + "," for v in value] + ["]"])
        raise TypeError(type(value))

    def _iter_norm(self, value: collections.abc.Sequence[str | Path | int]) -> str:
        if len(value) == 1:
            return self._norm(value[0])
        return "[" + ", ".join(self._norm(v) for v in value) + "]"

    def add_binary(
        self, name: str, command: Path | Command | collections.abc.Iterable[Path | Command]
    ) -> None:
        if name in self._binaries:
            raise KeyError(name)
        paths = [
            (part.path if isinstance(part, Command) else part)
            for part in ([command] if isinstance(command, Path | Command) else command)
        ]
        for path in paths:
            if not path.is_file():
                raise FileNotFoundError(path)
        self._binaries[name] = self._iter_norm(paths)

    def add_property(self, name: str, value: str | Path | int) -> None:
        if name in self._properties:
            raise KeyError(name)
        self._properties[name] = self._norm(value)

    def add_builtin_option(
        self, name: str, value: str | Path | int | collections.abc.Iterable[str | Path | int]
    ) -> None:
        if name in self._properties:
            raise KeyError(name)
        self._builtin_options[name] = self._norm(value)

    def save(self, target: Path) -> None:
        sections = {
            "binaries": self._binaries,
            "properties": self._properties,
            "built-in options": self._builtin_options,
        }

        with open(target, "w", encoding="utf-8") as f:
            for name, entries in sections.items():
                f.write(f"[{name}]\n")
                for k, v in entries.items():
                    f.write(f"{k} = {v}\n")
                f.write("\n")
