import functools
import math
import os
import shutil
import subprocess
import warnings
from pathlib import Path


def flatten(lists):
    """Given a possibly-recursive list, returned the flattened form"""
    result = []
    for x in lists:
        if isinstance(x, list):
            result.extend(flatten(x))
        else:
            result.append(x)
    return result


class Git:
    """Helper class for running Git queries and commands. Very incomplete."""

    def __new__(cls):
        """Return the singleton Git object, creating if needed."""
        if "_Git__singleton" not in cls.__dict__:
            cls.__singleton = super().__new__(cls)
            assert "_Git__singleton" in cls.__dict__
        return cls.__singleton

    def __init__(self):
        git = shutil.which("git")
        if not git:
            raise RuntimeError("Git is not available!")
        self.git: str = git

    def __call__(
        self, *args: str | Path, check: bool = True, supress_stderr: bool = False
    ) -> str | None:
        """Run a command and save the output."""
        result = subprocess.run(
            [self.git, *args],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL if supress_stderr else None,
            text=True,
            check=check,
        )
        return result.stdout.strip("\n") if result.returncode == 0 else None

    def list_paths(self, *args: str | Path, **kwargs) -> list[Path] | None:
        """Run a command who's output is a list of paths, and return the parsed output."""
        output = self(*args, **kwargs)
        if output is None:
            return None
        return [Path(line) for line in output.splitlines()] if output else []

    @functools.lru_cache
    def toplevel(self) -> Path:
        """Get the absolute path to the top-level Git directory, from git rev-parse --show-toplevel."""
        r = self("rev-parse", "--show-toplevel")
        return Path(r).resolve(strict=True)

    @functools.lru_cache
    def all_files(self, *args: str | Path, absolute: bool = False, **kwargs) -> list[Path]:
        """Get a list of all checked-in files as Git sees the world."""
        topdir = self.toplevel()
        relative = self.list_paths("-C", topdir, "ls-files", *args, **kwargs)
        if relative is None:
            return None
        return [topdir / p for p in relative] if absolute else relative

    @functools.lru_cache
    def is_file(self, *paths: Path) -> bool:
        """Check if any of the given paths is checked into the repository.
        See also all_files()."""
        return (
            self.all_files("--error-unmatch", *paths, check=False, supress_stderr=True) is not None
        )


def project_dir() -> Path:
    """Get the top-level project directory. Uses CI_PROJECT_DIR if available, otherwise uses the
    top-level Git directory."""
    if "CI_PROJECT_DIR" in os.environ:
        return Path(os.environ["CI_PROJECT_DIR"]).resolve(strict=True)
    return Git().toplevel()


def cpuset_max() -> int:
    """Get the maximum number of threads accessible according to the process affinity"""
    try:
        return len(os.sched_getaffinity(0))
    except Exception:  # Non-UNIX system
        return os.cpu_count()


def cgroup_max() -> float:
    """Get the maximum amount of CPU-time allowed by the active cgroups"""
    try:
        cgroup_fs = None
        with open("/proc/self/mountinfo", encoding="utf-8") as f:
            for mount in f:
                parts = mount.split()
                # We only care about 2 fields in here
                mntpoint = parts[4]
                fstype = parts[parts.index("-", 6) + 1]
                if fstype == "cgroup2" or fstype.startswith("cgroup2."):
                    cgroup_fs = mntpoint
                    break
        if cgroup_fs is None:
            # Must not be using cgroups v2, consider it unlimited
            warnings.warn("No cgroup2 filesystem mounted, ignoring cgroups CPU limits")
            return float("inf")
        assert isinstance(cgroup_fs, str)

        cgroup_path = None
        with open("/proc/self/cgroup", encoding="utf-8") as f:
            for cgroup in f:
                cgid, cnts, path = cgroup.split(":", 2)
                if cgid == "0" and len(cnts) == 0:
                    cgroup_path = path.strip()
                    break
        if cgroup_path is None:
            # Must not be in a cgroup? Consider it unlimited
            warnings.warn("Process is not part of a cgroup, ignoring cgroups CPU limits")
            return float("inf")
        assert isinstance(cgroup_path, str)

        with open(os.path.join(cgroup_fs + cgroup_path, "cpu.max"), encoding="utf-8") as f:
            for line in f:
                quota, period = line.split()
                if quota == "max":
                    # This really is unlimited
                    return float("inf")
                quota, period = int(quota), int(period)
                return quota / period
    except FileNotFoundError as e:
        warnings.warn(f"Failed to identify cgroups, ignoring cgroups CPU limits: {e}")
        return float("inf")


def nproc_max() -> int:
    """Get the maximum number of processes available to this execution, based on the limits"""
    return max(math.floor(min(cpuset_max(), cgroup_max())), 1)


def nproc() -> int:
    """Get the recommended number of processes that should be used for parallel operations"""
    np = nproc_max()
    match np:
        case 1:
            return 2  # Minor parallelism
        case 2:
            return 3  # Minor parallelism
        case _:
            return np
