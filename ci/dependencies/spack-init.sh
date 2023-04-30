#!/bin/bash

if test -z "$SPACK_ROOT"; then
  echo "SPACK_ROOT must be set externally!"
  exit 2
fi

# By default, we use a fork of Spack that has a few PRs fused together
spack_url=https://github.com/spack/spack.git
spack_ref=develop

echo -e "\e[0Ksection_start:$(date +%s):spack_clone_init[collapsed=true]\r\e[0KSpack update and initialization"

# Make sure we have a cached Spack
if ! [ -d .spack.git ]; then
  git clone --depth=30 --single-branch --no-tags --branch="$spack_ref" \
    --bare "$spack_url" .spack.git || exit $?
fi
git -C .spack.git remote set-url origin "$spack_url"

# Pull the latest, and then clone
git -C .spack.git fetch --verbose origin +"$spack_ref":"$spack_ref" || exit $?
git clone --shared --branch="$spack_ref" .spack.git "$SPACK_ROOT" || exit $?

# Set some standard configuration values for Spack in CI
#   Locking can be slow with fuse-overlayfs, give it a little more time than usual to sort out
"$SPACK_ROOT"/bin/spack config --scope site add 'config:db_lock_timeout:60' || exit $?
#   The network can be pretty heavy sometimes, don't stop because of that
"$SPACK_ROOT"/bin/spack config --scope site add 'config:connect_timeout:600' || exit $?

echo -e "\e[0Ksection_end:$(date +%s):spack_clone_init\r\e[0K"
