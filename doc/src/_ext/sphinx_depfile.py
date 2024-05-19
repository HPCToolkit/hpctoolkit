# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

# pylint: disable=missing-module-docstring

import itertools
from pathlib import Path


def setup(app):
    """Setup configuration for depfile extension"""
    app.add_config_value("depfile", None, "env")
    app.add_config_value("stampfile", None, "env")
    app.connect("build-finished", write_files)
    return {
        "version": "0.1.0",
    }


def write_files(app, _):
    """Write the dependendy Makefile and/or stampfile according to Sphinx operations"""

    # If requested, write out the stampfile, which is just an empty file
    if app.env.config.stampfile is not None:
        Path(app.env.config.stampfile).write_bytes(b"")

    # If requested, generate a Makefile similar to GCC et. al.
    if app.env.config.depfile is not None:
        files = set()

        # Documents and any noted dependencies are dependencies of the build
        for doc in app.env.all_docs:
            files.add(Path(app.env.doc2path(doc)))
            for dep in app.env.dependencies.get(doc, ()):
                pdep = Path(app.env.doc2path(dep))

                # Sphinx seems to report images as <image>.rst here? Not sure if there's a
                # better way to get images, but strip the *.rst and see if it's there.
                if not pdep.exists() and pdep.suffix.endswith(".rst"):
                    pdep = pdep.with_suffix(pdep.suffix[:-4])

                # Only report the dependency if it actually exists
                if pdep.exists():
                    files.add(pdep)

        # Files that are part of the _static or _templates are also dependencies
        for path in itertools.chain(app.env.config.html_static_path, app.env.config.templates_path):
            files.update(Path(path).rglob("*"))

        # The Makefile format is simple, one rule marking dependencies and blank rules for deps
        primary_target = Path(app.env.config.stampfile or app.outdir)
        with Path(app.env.config.depfile).open("w", encoding="utf-8") as f:
            f.write(f"{primary_target!s}:")
            for file in files:
                f.write(f" {file!s}")
            for file in files:
                f.write(f"\n{file!s}")
            f.write("\n")
