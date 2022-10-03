#!/usr/bin/env python3

# Utility much like nproc, but aware enough to read cgroups information

import math
import os
import sys


def cpuset_max():
    try:
        return len(os.sched_getaffinity(0))
    except Exception:  # Non-UNIX system
        return os.cpu_count()


def get_cgroup_fs():
    with open("/proc/self/mountinfo", encoding="utf-8") as f:
        for mount in f:
            parts = mount.split()
            # We only care about 2 fields in here
            mntpoint = parts[4]
            fstype = parts[parts.index("-", 6) + 1]
            if fstype == "cgroup2" or fstype.startswith("cgroup2."):
                return mntpoint
    raise RuntimeError("No cgroup2 filesystem mounted!")


def get_cgroup_path():
    with open("/proc/self/cgroup", encoding="utf-8") as f:
        for cgroup in f:
            cgid, cnts, path = cgroup.split(":", 2)
            if cgid == "0" and len(cnts) == 0:
                return path.strip()
    raise RuntimeError("Process is not part of a cgroup!")


def cgroup_max():
    try:
        cgroup = get_cgroup_fs() + get_cgroup_path()
        with open(os.path.join(cgroup, "cpu.max"), encoding="utf-8") as f:
            for line in f:
                quota, period = line.split()
                quota, period = int(quota), int(period)
                return quota / period
    except (FileNotFoundError, RuntimeError) as e:
        print(f"Failed to identify cgroups: {e}", file=sys.stderr)
        return float("inf")


def nproc():
    return max(math.floor(min(cpuset_max(), cgroup_max())), 1)


if __name__ == "__main__":
    result = nproc()
    for arg in sys.argv[1:]:
        result = min(result, int(arg))
    print(result)
