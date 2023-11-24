#!/bin/sh -e

test -n "$VERMODE"

here="$(realpath "$(dirname "$0")")"

# Concretize and install the environment
spack -D "$here" concretize
spack -D "$here" env depfile --generator make --make-prefix predeps -o "$here"/spack.mk
make -j "$(nproc)" -C "$here" --output-sync=recurse SPACK_INSTALL_FLAGS=--no-check-signature

# Clean up any build-only dependencies we won't need
spack -D "$here" gc -y

# Install Jinja2 in a temporary venv
"$(spack python --path)" -m venv /tmp/venv
/tmp/venv/bin/python -m pip install 'Jinja2>=3.0,<4'

# Generate a spack_env.ini Meson native file for the installed packages
/tmp/venv/bin/python "$here"/j2-spack.py "$here"/spack_env.ini.j2 "$here"/spack_env.ini

# Clean up
rm -r /tmp/venv
