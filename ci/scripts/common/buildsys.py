from pathlib import Path
import contextlib
import subprocess
import shutil
import re

from .logs import AbstractStatusResult

from .configuration import Configuration


def configure(
    srcdir, builddir, cfg: Configuration, prefix=None, logprefix="configure", logdir=None
):
    """Configure a build directory based on a Configuration, possibly redirecting logs.
    Returns True if the configuration was successful."""
    srcdir, builddir = Path(srcdir), Path(builddir)
    assert (srcdir / "configure").is_file()
    builddir.mkdir()
    if logdir is not None:
        logdir = Path(logdir)
        logdir.mkdir(parents=True, exist_ok=True)
        config_log = open(logdir / (logprefix + ".log"), "w")
    else:
        config_log = contextlib.nullcontext()

    extra_args = []
    if prefix is not None:
        extra_args.append(f"--prefix={Path(prefix).as_posix()}")

    with config_log as config_log:
        proc = subprocess.run(
            [srcdir / "configure"] + cfg.args + extra_args,
            cwd=builddir,
            stdout=config_log,
            stderr=config_log,
            env=cfg.env,
        )
    if logdir is not None and (builddir / "config.log").exists():
        shutil.copyfile(builddir / "config.log", logdir / "config.log")

    return proc.returncode == 0


def _make(
    builddir,
    cfg: Configuration,
    *makeargs,
    jobs=1,
    logdir=None,
    logprefix="make",
    split_stderr=True,
):
    """Run make in the given build directory"""
    builddir = Path(builddir)
    assert builddir.is_dir()

    if logdir is not None:
        logdir = Path(logdir)
        if split_stderr:
            out_log = open(logdir / (logprefix + ".stdout.log"), "w")
            err_log = open(logdir / (logprefix + ".stderr.log"), "w")
        else:
            out_log = open(logdir / (logprefix + ".log"), "w")
            err_log = contextlib.nullcontext(out_log)
    else:
        out_log = contextlib.nullcontext()
        err_log = contextlib.nullcontext()

    with out_log as out_log:
        with err_log as err_log:
            proc = subprocess.run(
                [cfg.make, "--output-sync", f"-j{jobs:d}" if jobs > 0 else "-j", *makeargs],
                cwd=builddir,
                stdout=out_log,
                stderr=err_log,
                env=cfg.env,
            )

    return proc.returncode == 0


def compile(builddir, cfg: Configuration, *, jobs=16, logprefix="compile", **kwargs):
    """Build the configured build directory, possibly redirecting logs.
    Returns True if the build was successful."""
    return _make(builddir, cfg, jobs=jobs, logprefix=logprefix, **kwargs)


def install(builddir, cfg: Configuration, *, jobs=1, logprefix="install", **kwargs):
    """Install the configured build directory, possibly redirecting logs.
    Returns True if the install was successful."""
    return _make(builddir, cfg, "install", jobs=jobs, logprefix=logprefix, **kwargs)


def test(builddir, cfg: Configuration, *, jobs=16, logprefix="test", logdir=None, **kwargs):
    """Run build-time tests for the configured build directory, possibly redirecting logs.
    Returns True if the test *running* was successful."""
    ok = _make(builddir, cfg, "check", jobs=jobs, logprefix=logprefix, logdir=logdir, **kwargs)
    if logdir is not None:
        testlogdir = Path(logdir) / logprefix
        testlogdir.mkdir()
        testsdir = builddir / "tests"
        for log in testsdir.rglob("*.log"):
            rlog = log.relative_to(testsdir)
            (testlogdir / rlog.parent).mkdir(parents=True, exist_ok=True)
            shutil.copyfile(log, testlogdir / rlog)
    return ok


class CompileResult(AbstractStatusResult):
    """Detect warnings/errors in compilation logs"""

    def __init__(self, logfile):
        self.warnings, self.errors = 0, 0
        with open(logfile, "r") as f:
            for line in f:
                if re.match(r"[^:]+:(\d+:){1,2}\s+warning:", line):  # Warning from GCC
                    self.warnings += 1
                elif re.match(r"[^:]+:(\d+:){1,2}\s+error:", line):  # Error from GCC
                    self.errors += 1
                elif re.match(r"[^:]+:\s+undefined reference to", line):  # Error from ld
                    self.errors += 1

    @property
    def flawless(self):
        return self.errors == 0 and self.warnings == 0

    def summary(self):
        if self.errors > 0:
            return f"detected {self.errors:d} errors + {self.warnings:d} warnings"
        if self.warnings > 0:
            return f"detected {self.warnings:d} warnings"
        return ""
