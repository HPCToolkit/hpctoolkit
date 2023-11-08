import functools
import shutil
import subprocess
import sys
from pathlib import Path

from jinja2 import Environment, FileSystemLoader


@functools.cache
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


if __name__ == "__main__":
    env = Environment(
        loader=FileSystemLoader("/"),
        keep_trailing_newline=True,
    )
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True)
    output.write_text(
        env.get_template(sys.argv[1]).render(spack_prefix=spack_prefix),
        encoding="utf-8",
    )
