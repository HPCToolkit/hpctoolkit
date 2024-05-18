#!/usr/bin/bash

# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

help() {
  echo "Usage: $0 [-d ENV/] [-c MIRSPEC] [-b MIRSPEC] [-S MIRSPEC] [-k] [-j FILE] [VERSION]

Arguments:
  -d ENV/     Save the environment to ENV/ instead of a temporary directory
  -c MIRSPEC  Add the given mirror to the environment before concretizing
  -b MIRSPEC  Add the given mirror to the environment before build/installing
  -S MIRSPEC  Add the given mirror to --scope=site before build/installing
  -k          Install buildcache signing keys before using them
  -j FILE     Generate JUnit output for Spack tests and write to FILE
  VERSION     Spack version of hpctoolkit to install and test

MIRSPEC above is a string of arguments to pass to 'spack mirror add', or:
  @SPACK_TAG        The latest tagged Spack version (via git describe, usually a develop-* snapshot)
  @GIT_HPCTOOLKIT   Source mirror containing a git archive of the current directory as an
                    hpctoolkit source tarball (of the same commit sha).

Exit codes:
  2      Argument syntax errors
  44     Test or hpctoolkit installation errors
  77     Other Spack errors (that usually can be ignored)
  Other  Script bugs
"
}

mirror_parse() {
  mirror_args=()
  case "$1" in
  @DEVELOP_SNAPSHOT)
    local desc
    if desc="$(git -C "$SPACK_ROOT" describe --tags)"; then
      mirror_args=(
        --signed --type binary spack-tagged
        https://binaries.spack.io/"$(echo "$desc" | sed -E 's/-[0-9]+-g[0-9a-f]+$//')"
      )
    fi
    ;;
  @GIT_HPCTOOLKIT)
    mkdir -p "$roottmp"/git-hpctoolkit-mirror/git/hpctoolkit/hpctoolkit.git/ || exit $?
    local sha
    sha="$(git -C "$here" rev-parse HEAD)" || exit $?
    git -C "$here" archive --output "$roottmp"/git-hpctoolkit-mirror/git/hpctoolkit/hpctoolkit.git/"$sha".tar.gz "$sha" || exit $?
    mirror_args=(--type source git-hpctoolkit "$roottmp"/git-hpctoolkit-mirror/)
    ;;
  @*)
    echo "Unrecognized mirror code: $1" >&2
    exit 2
    ;;
  *)
    eval "mirror_args=($1)"
    ;;
  esac
}

env_dir=
concrete_mirrors=()
build_mirrors=()
secret_mirrors=()
do_buildcache_keys_install=
junit_file=
while getopts "d:c:b:S:kj:" opt; do
  case "$opt" in
  d)
    env_dir="$(realpath "$OPTARG")"
    ;;
  c)
    concrete_mirrors+=("$OPTARG")
    ;;
  b)
    build_mirrors+=("$OPTARG")
    ;;
  S)
    secret_mirrors+=("$OPTARG")
    ;;
  k)
    do_buildcache_keys_install=yes
    ;;
  j)
    junit_file="$OPTARG"
    ;;
  *)
    help >&2
    exit 2
    ;;
  esac
done
shift $((OPTIND-1))
if ! [ -f "$env_dir"/spack.yaml ]; then
  version="$1"
  if [ -z "$version" ]; then
    echo "$0: Missing version" >&2
    help >&2
    exit 2
  fi
  shift
fi
if [ "$#" -gt 0 ]; then
  echo "$0: Unexpected arguments: $*" >&2
  help >&2
  exit 2
fi


here="$(realpath "$(dirname "$0")")" || exit $?
# We need temporaries for a lot of the stuff we do in here
trap 'rm -rf "$roottmp"' EXIT
roottmp="$(mktemp -d)" || exit $?


# Detach the Git --global config from the real --global config, so we can make edits without fear.
if [ -f "$HOME/.gitconfig" ]; then
  cp "$HOME/.gitconfig" "$roottmp/gitconfig" || exit $?
elif [ -f "${XDG_CONFIG_HOME:=$HOME/.config}/git/config" ]; then
  cp "$XDG_CONFIG_HOME/git/config" "$roottmp/gitconfig" || exit $?
fi
export GIT_CONFIG_GLOBAL="$roottmp/gitconfig"
# Fetch GitLab merge request commits as well as branches/tags
git config --global remote.origin.fetch '+refs/merge-requests/*:refs/remotes/origin/merge-requests/*' || exit $?


# Disable Spack user/system configuration, work with just site and the environment
export SPACK_DISABLE_LOCAL_CONFIG=1
# Set SPACK_ROOT to the root as determined by the Spack script
SPACK_ROOT="$(spack location --spack-root)" || exit $?
export SPACK_ROOT


# Instantiate the Spack environment we'll be using to do tests
if [ -z "$env_dir" ]; then env_dir="$roottmp/env"; fi
if [ -f "$env_dir"/spack.yaml ]; then
  echo "Reusing already existing environment in $env_dir"
else
  mkdir -p "$env_dir"
  sed "s/{{VERSION}}/$version/g" "$here"/spack.yaml.j2 > "$env_dir"/spack.yaml
fi


# Add all the mirrors that are supposed to go in before concretization
for m in "${concrete_mirrors[@]}"; do
  mirror_parse "$m" || exit $?
  if [ "${#mirror_args}" -gt 0 ]; then
    spack -D "$env_dir" mirror add "${mirror_args[@]}" || true
  fi
done
if [ "$do_buildcache_keys_install" ]; then
  spack -D "$env_dir" buildcache keys --install --trust || exit $?
fi


if [ -f "$env_dir"/spack.lock ]; then
  echo "Reusing concretization already present in $env_dir"
else
  # Concretize the environment based on the current state. We need to do this first because there's
  # currently no way to configure which buildcache we reuse specs from.
  spack -D "$env_dir" mirror list
  spack -D "$env_dir" concretize --test=root || {
    echo -e "\n\n--- Spack exited with code $?. This probably isn't an error with the hpctoolkit recipe, so it will be ignored." >&2
    exit 77
  }
fi


# Add all the mirrors that go in before building/installing.
for m in "${build_mirrors[@]}"; do
  mirror_parse "$m" || exit $?
  if [ "${#mirror_args}" -gt 0 ]; then
    spack -D "$env_dir" mirror add "${mirror_args[@]}" || true
  fi
done
# FIXME: Spack saves mirror authentication credentials in the mirrors section itself, rather than
# in a separate file. This means we need to duplicate mirrors, one unauthenticated mirror that gets
# saved in the environment (-b), and one authenticated mirror into a "secret" config location (-S).
for m in "${secret_mirrors[@]}"; do
  mirror_parse "$m" || exit $?
  if [ "${#mirror_args}" -gt 0 ]; then
    spack mirror add --scope=site "${mirror_args[@]}" || true
  fi
done
if [ "$do_buildcache_keys_install" ]; then
  spack -D "$env_dir" buildcache keys --install --trust || exit $?
fi


# Install everything except HPCToolkit itself. We do this first to ignore build errors that have
# nothing to do with us directly.
spack -D "$env_dir" install --fail-fast --only=dependencies || {
  echo -e "\n\n--- Spack exited with code $?. This probably isn't an error with the hpctoolkit recipe, so it will be ignored." >&2
  exit 77
}
if spack -D "$env_dir" mirror list | grep -q '^__emul_autopush '; then
  # FIXME: OCI mirrors don't work with --autopush. Until this is resolved, automatically push the
  # entire environment to a mirror called "__emul_autopush", if it exists.
  spack -D "$env_dir" buildcache push --only=dependencies __emul_autopush || true
fi


# Install HPCToolkit and run the build-time tests.
spack -D "$env_dir" install --fail-fast --test=root || exit 44
if spack -D "$env_dir" mirror list | grep -q '^__emul_autopush '; then
  # FIXME: See prior FIXME
  spack -D "$env_dir" buildcache push __emul_autopush || true
fi


# Run the standalone tests registered in the Spack recipe
if [ "$junit_file" ]; then
  spack -D "$env_dir" test run --log-file "$junit_file" --log-format junit hpctoolkit || exit 44
else
  spack -D "$env_dir" test run hpctoolkit || exit 44
fi
