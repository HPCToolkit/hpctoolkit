#!/bin/bash

if test -z "$SPACK_ROOT"; then
  echo "SPACK_ROOT must be set externally!"
  exit 2
fi

# Make sure we have a cached Spack
if ! [ -d .spack.git ]; then
  git clone --depth=30 --single-branch --no-tags --branch=octo-all-ci-improvements \
    --bare https://github.com/blue42u/spack.git .spack.git || exit $?
fi
git -C .spack.git remote set-url origin https://github.com/blue42u/spack.git

if test -n "$SPACK_CHECKOUT_VERSION"; then
  # Hotfix SPACK_CHECKOUT_VERSION, because it tends to be borked
  echo "$SPACK_CHECKOUT_VERSION"
  if (echo "$SPACK_CHECKOUT_VERSION" | grep -oE '[0-9a-f]{32}' > sha); then
    SPACK_CHECKOUT_VERSION=$(cat sha)
  fi
  rm sha

  # Update the Spack if needed, and then clone
  if ! git -C .spack.git cat-file -e "$SPACK_CHECKOUT_VERSION"'^{commit}'; then
    git -C .spack.git fetch --verbose origin +octo-all-ci-improvements:octo-all-ci-improvements  || exit $?
  fi
  git clone --shared --branch=octo-all-ci-improvements .spack.git "$SPACK_ROOT" || exit $?
  git -C "$SPACK_ROOT" checkout --detach "$SPACK_CHECKOUT_VERSION" || exit $?
else
  # Pull the latest, and then clone
  git -C .spack.git fetch --verbose origin +octo-all-ci-improvements:octo-all-ci-improvements || exit $?
  git clone --shared --branch=octo-all-ci-improvements .spack.git "$SPACK_ROOT" || exit $?
fi
