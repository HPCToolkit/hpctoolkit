import collections
import functools
import os
import shlex
import subprocess
import sys
import typing
from pathlib import Path

from .errors import PredictableFailureError

if typing.TYPE_CHECKING:
    import collections.abc


def _subproc_run(arg0, cmd, *args, wrapper: "collections.abc.Iterable[str]" = (), **kwargs):
    msg = f"--- --- Running command: {' '.join([shlex.quote(str(s)) for s in [*wrapper] + [arg0] + cmd[1:]])}"
    print(msg, flush=True)
    print(msg, file=sys.stderr, flush=True)
    return subprocess.run(list(wrapper) + cmd, *args, check=False, **kwargs)


def _identify_mpiexec(ranks: int):
    if "HPCTOOLKIT_DEV_MPIEXEC" not in os.environ:
        raise RuntimeError("mpiexec not available, cannot continue! Run under meson devenv!")
    mpiexec = [*os.environ["HPCTOOLKIT_DEV_MPIEXEC"].split(";"), f"{ranks:d}"]

    def attempt(
        args: "collections.abc.Iterable[str]",
    ) -> typing.Optional["collections.abc.Iterable[str]"]:
        proc = subprocess.run(
            mpiexec + list(args) + ["echo"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return args if proc.returncode in (0, 77) else None

    # Make sure mpiexec actually works
    if attempt([]) is not None:
        pass  # Default settings work
    elif (args := attempt(["--oversubscribe"])) is not None:
        mpiexec.extend(args)
    else:
        raise RuntimeError("mpiexec appears to be non-functional!")

    # Count the number of ranks that come out and ensure we get the number we expect
    proc = subprocess.run(
        [*mpiexec, "echo", "!!RANK!!"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    ranks_got = proc.stdout.splitlines().count(b"!!RANK!!")
    if ranks_got != ranks:
        raise RuntimeError(
            f"mpiexec did not spawn the correct number of ranks: expected {ranks:d}, got {ranks_got:d}"
        )

    return mpiexec


class Measurements:
    def __init__(self, basedir):
        self.basedir = Path(basedir)

    _log_suffix = ".log"
    _profile_suffix = ".hpcrun"
    _trace_suffix = ".hpctrace"
    _suffixes = (_log_suffix, _profile_suffix, _trace_suffix)

    @functools.cached_property
    def thread_stems(self):
        return {x.stem for x in self.basedir.iterdir() if x.suffix in self._suffixes}

    def _get_file_path(self, suffixattr, stem):
        result = self.basedir / (stem + getattr(self.__class__, suffixattr))
        return result if result.is_file() else None

    logfile = functools.partialmethod(_get_file_path, "_log_suffix")
    profile = functools.partialmethod(_get_file_path, "_profile_suffix")
    tracefile = functools.partialmethod(_get_file_path, "_trace_suffix")

    def __str__(self):
        return f"{self.__class__.__name__}({self.basedir}, {len(self.thread_stems)} threads)"

    def check_standard(
        self,
        *,
        procs: int = 1,
        threads_per_proc: typing.Union[int, "collections.abc.Collection[int]"] = 1,
        traces: bool = False,
    ):
        threads = [
            procs * tpp
            for tpp in (
                threads_per_proc if not isinstance(threads_per_proc, int) else [threads_per_proc]
            )
        ]
        for trial in threads:
            if len(self.thread_stems) == trial:
                break
        else:
            str_threads = " or ".join([f"{t:d}" for t in threads])
            raise PredictableFailureError(
                f"Expected exactly {str_threads} threads, got {len(self.thread_stems)}"
            )

        logs_found = 0
        for t in self.thread_stems:
            logs_found += 1 if self.logfile(t) else 0
            if not self.profile(t):
                raise PredictableFailureError(f"Expected a profile for thread stem {t}")
            if self.tracefile(t):
                if not traces:
                    raise PredictableFailureError(f"Did not expect a trace for thread stem {t}")
            elif traces:
                raise PredictableFailureError(f"Expected a trace for thread stem {t}")

        if logs_found != procs:
            raise PredictableFailureError(f"Expected exactly {procs} log file, got {logs_found}")


class Database:
    def __init__(self, basedir):
        self.basedir = Path(basedir)

    def __str__(self):
        return f"{self.__class__.__name__}({self.basedir})"

    def check_standard(self, *, tracedb: bool = False):
        meta_db = self.basedir / "meta.db"
        profile_db = self.basedir / "profile.db"
        cct_db = self.basedir / "cct.db"
        trace_db = self.basedir / "trace.db"
        formats_md = self.basedir / "FORMATS.md"
        metrics_yaml = self.basedir / "metrics" / "METRICS.yaml.ex"
        metrics_default = self.basedir / "metrics" / "default.yaml"

        # Check for standard files that should always be there
        for fn in (meta_db, profile_db, cct_db, formats_md, metrics_yaml, metrics_default):
            if not fn.is_file():
                raise PredictableFailureError(f"Expected {fn.name} in database!")

        # trace.db should only be present if traces were enabled
        if trace_db.is_file():
            if not tracedb:
                raise PredictableFailureError("trace.db present in database but not expected!")
        elif tracedb:
            raise PredictableFailureError("Expected trace.db in database!")
