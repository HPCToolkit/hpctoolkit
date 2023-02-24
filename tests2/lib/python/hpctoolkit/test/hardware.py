import collections.abc
import contextlib
import csv
import os
import random
import re
import shutil
import subprocess
import sys


@contextlib.contextmanager
def nvidia(*capabilities: str, num_devices: int = 1) -> collections.abc.Iterator[None]:
    nv_smi = shutil.which("nvidia-smi")
    if nv_smi is None:
        print("SKIP: nvidia-smi not found, CUDA must not be available.")
        sys.exit(77)

    proc = subprocess.run(
        ["nvidia-smi", "--format=csv", "--query-gpu=index,uuid,compute_cap"],
        check=True,
        capture_output=True,
        text=True,
        stderr=None,
    )
    gpus: list[dict] = list(csv.DictReader(proc.stdout.split("\n"), skipinitialspace=True))
    print(gpus)

    # Try to obey CUDA_VISIBLE_DEVICES to some extent
    if "CUDA_VISIBLE_DEVICES" in os.environ:
        devs = os.environ["CUDA_VISIBLE_DEVICES"].split(",")
        gpus = [g for g in gpus if g["index"] in devs]

    for g in gpus:
        mat = re.fullmatch(r"(\d+)\.(\d+)", g["compute_cap"])
        if not mat:
            raise ValueError(g["compute_cap"])
        g["compute_cap_v"] = int(mat[1]), int(mat[2])

    for c in capabilities:
        mat = re.fullmatch(r"(>=|<=|=)(\d+)\.(\d+)", c)
        if not mat:
            raise ValueError(c)

        cver = (int(mat[2]), int(mat[3]))
        match mat[1]:
            case ">=":
                gpus = [g for g in gpus if g["compute_cap_v"] >= cver]

            case "<=":
                gpus = [g for g in gpus if g["compute_cap_v"] <= cver]

            case "=":
                gpus = [g for g in gpus if g["compute_cap_v"] == cver]

            case _:
                raise AssertionError

    if len(gpus) < num_devices:
        print(
            f"SKIP: Not enough available GPUs passed the preconditions ({len(gpus):d} < {num_devices:d})"
        )
        sys.exit(77)

    old_cvd: str | None = os.environ.get("CUDA_VISIBLE_DEVICES")
    os.environ["CUDA_VISIBLE_DEVICES"] = ",".join(
        g["index"] for g in random.choices(gpus, k=num_devices)
    )
    try:
        yield
    finally:
        if old_cvd is None:
            del os.environ["CUDA_VISIBLE_DEVICES"]
        else:
            os.environ["CUDA_VISIBLE_DEVICES"] = old_cvd
