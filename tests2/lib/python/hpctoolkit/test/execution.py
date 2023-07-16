import collections
import collections.abc
import contextlib
import functools
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

from .errors import PredictableFailureError


def _subproc_run(arg0, cmd, *args, wrapper: collections.abc.Iterable[str] = (), **kwargs):
    msg = f"--- --- Running command: {' '.join([shlex.quote(str(s)) for s in [*wrapper] + [arg0] + cmd[1:]])}"
    print(msg, flush=True)
    print(msg, file=sys.stderr, flush=True)
    return subprocess.run(list(wrapper) + cmd, *args, check=False, **kwargs)


def _identify_mpiexec(ranks: int):
    if "HPCTOOLKIT_DEV_MPIEXEC" not in os.environ:
        raise RuntimeError("mpiexec not available, cannot continue! Run under meson devenv!")
    mpiexec = [*os.environ["HPCTOOLKIT_DEV_MPIEXEC"].split(";"), f"{ranks:d}"]

    def attempt(args: collections.abc.Iterable[str]) -> collections.abc.Iterable[str] | None:
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

    def check_standard(self, *, procs: int = 1, threads_per_proc: int = 1, traces: bool = False):
        threads = procs * threads_per_proc
        if len(self.thread_stems) != threads:
            raise PredictableFailureError(
                f"Expected exactly {threads:d} threads, got {len(self.thread_stems)}"
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


@contextlib.contextmanager
def hpcrun(
    *hpcrun_args: str | None,
    cmd: collections.abc.Sequence[str],
    timeout: int | None = None,
    env=None,
    output=None,
):
    if "HPCTOOLKIT_APP_HPCRUN" not in os.environ:
        raise RuntimeError("hpcrun not available, cannot continue! Run under meson devenv!")
    hpcrun = os.environ["HPCTOOLKIT_APP_HPCRUN"]

    if env is not None:
        env = collections.ChainMap(env, os.environ)

    if output is not None:
        output = contextlib.nullcontext(Path(output))
    else:
        output = tempfile.TemporaryDirectory(prefix="hpc-tsuite-", suffix="-measurements")

    with output as mdir_str:
        mdir = Path(mdir_str)
        arglist = [hpcrun] + [a for a in hpcrun_args if a is not None] + ["-o", mdir, *cmd]
        proc = _subproc_run("hpcrun", arglist, timeout=timeout, env=env)
        m = Measurements(mdir)
        # Dump the log files for debugging purposes
        with contextlib.suppress(OSError):
            for t in m.thread_stems:
                if fn := m.logfile(t):
                    print(f"--- --- {fn.name}", file=sys.stderr, flush=True)
                    with open(fn, encoding="utf-8") as f:
                        for line in f:
                            print(line.rstrip(), file=sys.stderr)
            print("--- --- END LOGS", file=sys.stderr, flush=True)

        if proc.returncode != 0:
            raise PredictableFailureError("hpcrun returned a non-zero exit code!")

        yield m


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


@contextlib.contextmanager
def hpcprof(
    meas: Measurements | Path | str,
    *args: str,
    threads: int = 1,
    timeout: int | None = None,
    env=None,
    output=None,
):
    assert threads >= 0
    if isinstance(meas, Measurements):
        meas = meas.basedir
    meas = Path(meas)

    if "HPCTOOLKIT_APP_HPCPROF" not in os.environ:
        raise RuntimeError("hpcprof not available, cannot continue! Run under meson devenv!")
    hpcprof = os.environ["HPCTOOLKIT_APP_HPCPROF"]

    if env is not None:
        env = collections.ChainMap(env, os.environ)

    if output is not None:
        output = contextlib.nullcontext(Path(output))
    else:
        output = tempfile.TemporaryDirectory(prefix="hpctsuite-", suffix="-database")

    with output as ddir_str:
        ddir = Path(ddir_str)
        ddir.rmdir()
        proc = _subproc_run(
            "hpcprof",
            [hpcprof, *list(args), f"-j{threads:d}", "-o", ddir, meas],
            timeout=timeout,
            env=env,
        )
        if proc.returncode != 0:
            raise PredictableFailureError("hpcprof returned a non-zero exit code!")

        yield Database(ddir)


@contextlib.contextmanager
def hpcprof_mpi(
    ranks: int,
    meas: Measurements | Path | str,
    *args: str,
    threads: int = 1,
    timeout: int | None = None,
    env=None,
    output=None,
):
    assert threads >= 0
    if isinstance(meas, Measurements):
        meas = meas.basedir
    meas = Path(meas)

    if "HPCTOOLKIT_APP_HPCPROF_MPI" not in os.environ:
        raise RuntimeError("hpcprof-mpi not available, cannot continue! Run under meson devenv!")
    hpcprof_mpi = os.environ["HPCTOOLKIT_APP_HPCPROF_MPI"]

    mpiexec = _identify_mpiexec(ranks)

    if env is not None:
        env = collections.ChainMap(env, os.environ)

    if output is not None:
        output = contextlib.nullcontext(Path(output))
    else:
        output = tempfile.TemporaryDirectory(prefix="hpctsuite-", suffix="-database")

    with output as ddir_str:
        ddir = Path(ddir_str)
        ddir.rmdir()
        proc = _subproc_run(
            "hpcprof-mpi",
            [hpcprof_mpi, *list(args), f"-j{threads:d}", "-o", ddir, meas],
            timeout=timeout,
            env=env,
            wrapper=mpiexec,
        )
        if proc.returncode == 77:  # Propagate a skip returncode
            sys.exit(77)
        if proc.returncode != 0:
            raise PredictableFailureError("hpcprof returned a non-zero exit code!")

        yield Database(ddir)


def hpcstruct(
    binary_or_meas: Measurements | Path | str,
    *args: str,
    threads: int = 1,
    timeout: int | None = None,
    env=None,
    output=None,
):
    assert threads >= 0
    if "HPCTOOLKIT_APP_HPCSTRUCT" not in os.environ:
        raise RuntimeError("hpcstruct not available, cannot continue! Run under meson devenv!")
    hpcstruct = os.environ["HPCTOOLKIT_APP_HPCSTRUCT"]

    env_stack = [{"HPCTOOLKIT_HPCSTRUCT_CACHE": ""}, os.environ]
    if env is not None:
        env_stack = [env, *env_stack]
    env = collections.ChainMap(*env_stack)

    if isinstance(binary_or_meas, Measurements):
        assert output is None, "Output cannot be specified with a Measurements input"

        meas = binary_or_meas.basedir
        proc = _subproc_run(
            "hpcstruct",
            [hpcstruct, *list(args), f"-j{threads:d}", meas],
            timeout=timeout,
            env=env,
        )
        if proc.returncode != 0:
            raise PredictableFailureError("hpcstruct returned a non-zero exit code!")

        return None

    binary = Path(binary_or_meas)
    if not binary.is_file():
        raise ValueError("Input to hpcstruct must be a single binary or a Measurements")

    @contextlib.contextmanager
    def ctx():
        out = output

        if out is not None:
            out = contextlib.nullcontext(Path(out))
        else:
            out = tempfile.NamedTemporaryFile(prefix="hpctsuite-", suffix=".hpcstruct")

        with out as sfile:
            proc = _subproc_run(
                "hpcstruct",
                [hpcstruct, *list(args), f"-j{threads:d}", "-o", sfile.name, binary],
                timeout=timeout,
                env=env,
            )
            if proc.returncode != 0:
                raise PredictableFailureError("hpcstruct returned a non-zero exit code!")

            yield sfile

    return ctx()
