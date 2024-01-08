import functools
import shutil
import subprocess
import sys
import typing
from pathlib import Path

from jinja2 import Environment, FileSystemLoader


@functools.lru_cache(maxsize=None)
def spack_prefix_real(env: str, spec: str) -> str:
    spack = shutil.which("spack")
    if not spack:
        raise FileNotFoundError("spack")
    result = subprocess.run(
        [spack, "-D", env, "find", "--format={prefix}", spec],
        stdout=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        print(result.stdout.decode("utf-8"), file=sys.stderr, end="")
        return ""
    return result.stdout.decode("utf-8").strip()


def spack_prefix(spec: str) -> str:
    return spack_prefix_real("/opt/environment", spec)


@functools.lru_cache(maxsize=None)
def spack_all_specs_real(env: str) -> typing.List[str]:
    spack = shutil.which("spack")
    if not spack:
        raise FileNotFoundError("spack")
    result = subprocess.run(
        [spack, "-D", env, "find", "--format={/hash}"],
        stdout=subprocess.PIPE,
        check=True,
    )
    return result.stdout.decode("utf-8").split()


def spack_all_specs() -> typing.List[str]:
    return spack_all_specs_real("/opt/environment")


if __name__ == "__main__":
    env = Environment(
        loader=FileSystemLoader("/"),
        keep_trailing_newline=True,
    )
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        env.get_template(sys.argv[1]).render(
            spack_prefix=spack_prefix, spack_all_specs=spack_all_specs
        ),
        encoding="utf-8",
    )
