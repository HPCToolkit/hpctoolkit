#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

# pylint: disable=invalid-name,missing-module-docstring,duplicate-code

import shutil
import subprocess
import sys
from pathlib import Path

if __name__ == "__main__":
    build_dir, srcfile, dstfile, latexmk, texfile, sphinx_cmd = (
        sys.argv[1],
        sys.argv[2],
        sys.argv[3],
        sys.argv[4],
        sys.argv[5],
        sys.argv[6:],
    )
    src = Path(build_dir) / srcfile
    dst = Path(dstfile)

    dst.unlink(missing_ok=True)

    try:
        subprocess.run([*sphinx_cmd, build_dir], check=True)
        subprocess.run(
            [latexmk, "-pdf", "-dvi-", "-ps-", texfile], cwd=build_dir, check=True
        )
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)

    if not src.is_file():
        raise RuntimeError(f"Sphinx did not produce file: {src.as_posix()}")
    shutil.copy2(src, dst)
