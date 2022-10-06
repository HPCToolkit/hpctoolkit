import math
import os
import warnings


def flatten(lists):
    """Given a possibly-recursive list, returned the flattened form"""
    result = []
    for x in lists:
        if isinstance(x, list):
            result.extend(flatten(x))
        else:
            result.append(x)
    return result


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
