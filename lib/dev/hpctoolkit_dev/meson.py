from pathlib import Path


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

    def _norm(self, value: str | Path | int) -> str:
        if isinstance(value, int):
            return f"{value:d}"
        if isinstance(value, (str, Path)):
            return "'{}'".format(str(value).replace("'", "\\'"))
        return None

    def add_binary(self, name: str, path: Path) -> None:
        if name in self._binaries:
            raise KeyError(name)
        if not path.is_file():
            raise FileNotFoundError(path)
        self._binaries[name] = self._norm(path)

    def add_property(self, name: str, value: str | Path | int) -> None:
        if name in self._properties:
            raise KeyError(name)
        self._properties[name] = self._norm(value)

    def save(self, target: Path) -> None:
        sections = {
            "binaries": self._binaries,
            "properties": self._properties,
        }

        with open(target, "w", encoding="utf-8") as f:
            for name, entries in sections.items():
                f.write(f"[{name}]\n")
                for k, v in entries.items():
                    f.write(f"{k} = {v}\n")
                f.write("\n")
