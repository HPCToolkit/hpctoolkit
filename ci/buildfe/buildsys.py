import contextlib
import re
import shutil
import subprocess
import typing as T
from pathlib import Path

from .action import Action, ActionResult, ReturnCodeResult
from .configuration import Configuration
from .logs import dump_file
from .util import nproc


@contextlib.contextmanager
def _stdlogfiles(logdir: Path, logprefix: str, suffix1: str, suffix2: T.Optional[str] = None):
    if logdir is not None:
        logdir = Path(logdir)
        with open(logdir / (logprefix + suffix1), "w", encoding="utf-8") as f1:
            if suffix2 is not None:
                with open(logdir / (logprefix + suffix2), "w", encoding="utf-8") as f2:
                    yield f1, f2
            else:
                yield f1, f1
    else:
        yield None, None


class Configure(Action):
    """Configure a build directory based on a Configuration."""

    def name(self) -> str:
        return "./configure"

    def dependencies(self) -> tuple[Action]:
        return tuple()

    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: T.Optional[Path] = None,
    ) -> ActionResult:
        assert (srcdir / "configure").is_file()
        builddir.mkdir()

        cmd = [srcdir / "configure"] + cfg.args + [f"--prefix={installdir.as_posix()}"]
        with _stdlogfiles(logdir, "configure", ".log") as (config_log, _):
            proc = subprocess.run(
                cmd, cwd=builddir, stdout=config_log, stderr=config_log, env=cfg.env, check=False
            )
        if logdir is not None and (builddir / "config.log").exists():
            shutil.copyfile(builddir / "config.log", logdir / "configure.config.log")

        if proc.returncode != 0 and logdir is not None:
            dump_file(logdir / "configure.log")

        return ReturnCodeResult("configure", proc.returncode)


class MakeAction(Action):
    """Base class for Actions that primarily run `make ...` in the build directory."""

    def _run(
        self,
        logprefix: str,
        cfg: Configuration,
        builddir: Path,
        *targets,
        logdir: T.Optional[Path] = None,
        split_stderr: bool = True,
        parallel: bool = True,
    ) -> ActionResult:
        assert builddir.is_dir()

        make = shutil.which("make")
        if make is None:
            raise RuntimeError("Unable to find Make!")
        assert isinstance(make, str)

        cmd = [make, "--output-sync", f"-j{nproc() if parallel else 1:d}", *targets]
        logsuffixes = (".stdout.log", ".stderr.log") if split_stderr else (".log",)
        with _stdlogfiles(logdir, logprefix, *logsuffixes) as (out_log, err_log):
            proc = subprocess.run(
                cmd, cwd=builddir, stdout=out_log, stderr=err_log, env=cfg.env, check=False
            )

        if logdir is not None:
            dump_file(logdir / (logprefix + logsuffixes[-1]))

        return ReturnCodeResult(" ".join(["make", *targets]), proc.returncode)


class BuildResult(ActionResult):
    """Detect warnings/errors in build logs"""

    def __init__(self, logfile: Path, subresult: ActionResult):
        self.subresult = subresult
        self.warnings, self.errors = 0, 0
        with open(logfile, encoding="utf-8") as f:
            for line in f:
                if re.match(r"[^:]+:(\d+:){1,2}\s+warning:", line):  # Warning from GCC
                    self.warnings += 1
                elif re.match(r"[^:]+:(\d+:){1,2}\s+error:", line):  # Error from GCC
                    self.errors += 1
                elif re.match(r"[^:]+:\s+undefined reference to", line):  # Error from ld
                    self.errors += 1

    @property
    def completed(self):
        return self.subresult.completed

    @property
    def passed(self):
        return self.subresult.passed and self.errors == 0

    @property
    def flawless(self):
        return self.subresult.flawless and self.errors == 0 and self.warnings == 0

    def summary(self):
        fragments = []
        if self.errors > 0:
            fragments.append(
                f"detected {self.errors:d} errors + {self.warnings:d} warnings during build"
            )
        elif self.warnings > 0:
            fragments.append(f"detected {self.warnings:d} warnings during build")

        if not self.subresult.flawless:
            fragments.append(self.subresult.summary())

        return ", ".join(fragments) if fragments else self.subresult.summary()


class UnscannedBuildResult(ActionResult):
    """Result used when the build logs were not saved and thus not scanned"""

    def __init__(self, subresult: ActionResult):
        self.subresult = subresult

    @property
    def completed(self):
        return True

    @property
    def passed(self):
        return False

    @property
    def flawless(self):
        return False

    def summary(self):
        return (
            "build logs not saved, error detection skipped"
            if self.subresult.flawless
            else self.subresult.summary()
        )


class Build(MakeAction):
    """Build the configured build directory."""

    def name(self) -> str:
        return "make all"

    def dependencies(self) -> tuple[Action]:
        return (Configure(),)

    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: T.Optional[Path] = None,
    ) -> ActionResult:
        res = self._run("build", cfg, builddir, "all", logdir=logdir, split_stderr=True)
        return (
            BuildResult(logdir / "build.stderr.log", res)
            if logdir is not None
            else UnscannedBuildResult(res)
        )


class Install(MakeAction):
    """Install the configured build directory."""

    def name(self) -> str:
        return "make install"

    def dependencies(self) -> tuple[Action]:
        return (Build(),)

    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: T.Optional[Path] = None,
    ) -> ActionResult:
        return self._run("install", cfg, builddir, "install", logdir=logdir, split_stderr=True)


class CheckManifestResult(ActionResult):
    """Result from checking against the expected install manifest."""

    def __init__(self, missing: int, unexpected: int):
        self.missing = missing
        self.unexpected = unexpected

    @property
    def completed(self):
        return True

    @property
    def passed(self):
        return self.missing == 0

    @property
    def flawless(self):
        return self.passed and self.unexpected == 0

    def summary(self):
        if self.missing > 0:
            return f"{self.missing:d} files not installed + {self.unexpected:d} unexpected installed files"
        if self.unexpected > 0:
            return f"{self.unexpected:d} unexpected installed files"
        return "install manifest matched"


class CheckInstallManifest(Action):
    """Check the installed files against the expected install manifest."""

    def header(self, cfg: Configuration) -> str:
        return "Checking install manifest..."

    def name(self) -> str:
        return "check install"

    def dependencies(self) -> tuple[Action]:
        return (Install(),)

    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: T.Optional[Path] = None,
    ) -> ActionResult:
        return CheckManifestResult(*cfg.manifest.check(installdir))


class Test(MakeAction):
    """Run build-time tests for the configured build directory."""

    def name(self) -> str:
        return "make check installcheck"

    def dependencies(self) -> tuple[Action]:
        return Build(), Install()

    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: T.Optional[Path] = None,
    ) -> ActionResult:
        res = self._run("test", cfg, builddir, "check", "installcheck", logdir=logdir)
        if logdir is not None:
            testlogdir = logdir / "test"
            testlogdir.mkdir()
            testsdir = builddir / "tests"
            for log in testsdir.rglob("*.log"):
                rlog = log.relative_to(testsdir)
                (testlogdir / rlog.parent).mkdir(parents=True, exist_ok=True)
                shutil.copyfile(log, testlogdir / rlog)
            tests2bdir = builddir / "tests2-build"
            shutil.copyfile(
                tests2bdir / "meson-logs" / "testlog.txt", testlogdir / "test.tests2.log"
            )
            for fn in (
                tests2bdir / "meson-logs" / "testlog.junit.xml",
                tests2bdir / "lib" / "python" / "hpctoolkit" / "pytest.junit.xml",
            ):
                if fn.exists():
                    shutil.copyfile(fn, logdir / ("test." + fn.name))
        return res


class GenTestData(MakeAction):
    """Generate data for later, offline tests"""

    def name(self) -> str:
        return "make gen-testdata"

    def dependencies(self) -> tuple[Action]:
        return Build(), Install()

    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: T.Optional[Path] = None,
    ) -> ActionResult:
        return self._run("gen-testdata", cfg, builddir, "gen-testdata", logdir=logdir)
