import collections
import collections.abc
import os
import shutil
import subprocess
import sys
import typing
from pathlib import Path

__all__ = ["Command"]


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
                sys.stdout.flush()
            raise

    def capture(
        self,
        /,
        *args: str | Path,
        env: collections.abc.Mapping[str, str] | None = None,
        stdinput: str | None = None,
        cwd: Path | None = None,
        error_output: bool = True,
    ) -> str:
        """Run the Command with the given arguments and options, and return it's stdout."""
        cmd = [self.path, *self._initial_args, *args]
        try:
            proc = subprocess.run(
                cmd,
                check=True,
                text=True,
                stdout=subprocess.PIPE,
                stderr=None if error_output else subprocess.DEVNULL,
                env=env,
                input=stdinput,
                cwd=cwd,
            )
        except subprocess.CalledProcessError as e:
            if error_output:
                sys.stdout.write(e.stdout)
                sys.stdout.flush()
            raise
        return proc.stdout

    def execl(
        self,
        /,
        *args: str | Path,
        env: collections.abc.Mapping[str, str] | None = None,
        cwd: Path | None = None,
    ) -> typing.NoReturn:
        """Run the Command, replacing the current process with it."""
        cmd = [self.path, *self._initial_args, *args]
        if cwd is not None:
            os.chdir(cwd)
        os.execve(self.path, [str(a) for a in cmd], env if env is not None else os.environ)
