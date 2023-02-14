#!/bin/bash

if test -z "$SPACK_ROOT"; then
  echo "SPACK_ROOT must be set externally!"
  exit 2
fi

# By default, we use a fork of Spack that has a few PRs fused together
spack_url=https://github.com/blue42u/spack.git
spack_ref=octo-all-ci-improvements
if test -n "$1"; then
  spack_url="$1"
  spack_ref="$2"
fi

echo -e "\e[0Ksection_start:$(date +%s):spack_clone_init[collapsed=true]\r\e[0KSpack update and initialization"

# Make sure we have a cached Spack
if ! [ -d .spack.git ]; then
  git clone --depth=30 --single-branch --no-tags --branch="$spack_ref" \
    --bare "$spack_url" .spack.git || exit $?
fi
git -C .spack.git remote set-url origin "$spack_url"

if test -n "$SPACK_CHECKOUT_VERSION"; then
  # Hotfix SPACK_CHECKOUT_VERSION, because it tends to be borked
  echo "$SPACK_CHECKOUT_VERSION"
  if (echo "$SPACK_CHECKOUT_VERSION" | grep -oE '[0-9a-f]{32}' > sha); then
    SPACK_CHECKOUT_VERSION=$(cat sha)
  fi
  rm sha

  # Update the Spack if needed, and then clone
  if ! git -C .spack.git cat-file -e "$SPACK_CHECKOUT_VERSION"'^{commit}'; then
    git -C .spack.git fetch --verbose origin +"$spack_ref":"$spack_ref"  || exit $?
  fi
  git clone --shared --branch="$spack_ref" .spack.git "$SPACK_ROOT" || exit $?
  git -C "$SPACK_ROOT" checkout --detach "$SPACK_CHECKOUT_VERSION" || exit $?
else
  # Pull the latest, and then clone
  git -C .spack.git fetch --verbose origin +"$spack_ref":"$spack_ref" || exit $?
  git clone --shared --branch="$spack_ref" .spack.git "$SPACK_ROOT" || exit $?
fi

# Set some standard configuration values for Spack in CI
#   Locking can be slow with fuse-overlayfs, give it a little more time than usual to sort out
"$SPACK_ROOT"/bin/spack config --scope site add 'config:db_lock_timeout:60' || exit $?
#   The network can be pretty heavy sometimes, don't stop because of that
"$SPACK_ROOT"/bin/spack config --scope site add 'config:connect_timeout:600' || exit $?

echo -e "\e[0Ksection_end:$(date +%s):spack_clone_init\r\e[0K"
