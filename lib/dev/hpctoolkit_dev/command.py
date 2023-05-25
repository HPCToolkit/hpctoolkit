import collections
import collections.abc
import contextlib
import os
import re
import shlex
import shutil
import subprocess
import sys
import typing
from pathlib import Path

__all__ = ("Command", "ShEnv")


class Command:
    """Callable convenience wrapper for an executable command."""

    def __init__(
        self,
        command: str | os.PathLike,
        *args: str | Path,
    ) -> None:
        """Create a new Command wrapping the given executable, with some arguments."""
        self.path = Path(command)
        if not self.path.is_file():
            raise ValueError(self.path)
        self._initial_args = list(args)

    def withargs(
        self,
        *args: str | Path,
    ) -> "Command":
        """Return a copy of self with additional arguments added to the default sets."""
        final_args = self._initial_args + list(args)
        return self.__class__(self.path, *final_args)

    @classmethod
    def which(cls, command: str, *, path: str | None = None) -> "Command":
        """Create a new Command wrapping a command present on the PATH (or path). Functionally
        a wrapper for shutil.which().
        """
        realcmd = shutil.which(command, path=path)
        if realcmd is None:
            raise ValueError(f"Could not find command {command}")
        return cls(realcmd)

    def __call__(
        self,
        /,
        *args: str | Path,
        env: collections.abc.Mapping[str, str] | None = None,
        stdinput: str | None = None,
        output: bool = True,
        cwd: Path | None = None,
    ) -> None:
        """Run the Command with the given arguments and options."""
        cmd = [self.path, *self._initial_args, *args]
        try:
            subprocess.run(
                cmd,
                check=True,
                env=env,
                text=True,
                input=stdinput,
                stdout=None if output else subprocess.PIPE,
                cwd=cwd,
            )
        except subprocess.CalledProcessError as e:
            if e.stdout:
                sys.stdout.write(e.stdout)
            raise

    def capture(
        self,
        /,
        *args: str | Path,
        env: collections.abc.Mapping[str, str] | None = None,
        stdinput: str | None = None,
        cwd: Path | None = None,
    ) -> str:
        """Run the Command with the given arguments and options, and return it's stdout."""
        cmd = [self.path, *self._initial_args, *args]
        try:
            proc = subprocess.run(
                cmd,
                check=True,
                text=True,
                stdout=subprocess.PIPE,
                env=env,
                input=stdinput,
                cwd=cwd,
            )
        except subprocess.CalledProcessError as e:
            sys.stdout.write(e.stdout)
            raise
        return proc.stdout


class ShEnv(collections.abc.Mapping[str, str]):
    """Parsed table of environment variables, deciphered from the shell generated by `spack load --sh`."""

    def __init__(self, code: str) -> None:
        self._raw = code
        self._env: dict[str, str] = {}
        lexer = shlex.shlex(code, punctuation_chars=True, posix=True)
        lexer.whitespace_split = True

        command = None
        for token in lexer:
            if token == ";":
                command = None
                continue

            match command:
                case None:
                    if token != "export":
                        raise ValueError(f"Only allowed command is export, got: {token!r}")
                    command = token

                case "export":
                    parts = token.split("=", maxsplit=1)
                    if len(parts) != 2:
                        raise ValueError(f"exported variables must have values, got: {token!r}")
                    self._env[parts[0]] = parts[1]

    def extend(self, env: collections.abc.Mapping[str, str]) -> "ShEnv":
        """Create a new ShEnv based on this one, adding the variables set in env."""
        new_code = [self._raw]
        for key, val in env.items():
            if not re.fullmatch(r"\w+", key):
                raise ValueError(key)
            new_code.append(f"export {key}={shlex.quote(val)};")
        return self.__class__("\n".join(new_code))

    def to_environ(self) -> dict[str, str]:
        """Generate the final environ processes within this space should use."""
        result: dict[str, str] = {}
        result.update(os.environ)
        result.update(self)
        return result

    @contextlib.contextmanager
    def activate(self) -> typing.Iterator[None]:
        old_env = dict(os.environ)
        try:
            os.environ.update(self)
            yield
        finally:
            os.environ.clear()
            os.environ.update(old_env)

    @property
    def raw(self) -> str:
        """Fetch the raw shell code that this table was generated from."""
        return self._raw

    def __contains__(self, key) -> bool:
        return key in self._env

    def __len__(self) -> int:
        return len(self._env)

    def __getitem__(self, key) -> str:
        return self._env[key]

    def __iter__(self) -> typing.Iterator[str]:
        return iter(self._env)