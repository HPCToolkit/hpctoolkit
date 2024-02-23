import os
import subprocess
import sys
import tempfile
from pathlib import Path

tmpdir = None
if "MESON_BUILD_ROOT" in os.environ:
    tmpdir = Path(os.environ["MESON_BUILD_ROOT"])
    if "MESON_SUBDIR" in os.environ:
        tmpdir /= os.environ["MESON_SUBDIR"]

with tempfile.TemporaryDirectory(dir=tmpdir) as d:
    proc = subprocess.run(sys.argv[1:], cwd=d, check=False)

sys.exit(proc.returncode)
