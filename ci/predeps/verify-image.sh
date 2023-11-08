#!/bin/bash -e

srcdir="$1"
builddir="$2"

msetup() {
  # Usage: msetup <native files>... -D<option>...
  local nativeargs=()
  while [ "$1" ]; do
    if [[ "$1" =~ ^- ]]; then break; else
      if ! [ -e /usr/share/meson/native/"$1".ini ]; then
        return 0
      fi
      nativeargs+=(--native-file "$1".ini)
    fi
    shift
  done

  meson setup --wipe "$builddir" "$srcdir" -Dauto_features=disabled \
    -Dpapi=enabled -Dopencl=enabled -Dhpcprof_mpi=enabled \
    --native-file image.ini "${nativeargs[@]}" "$@" \
  || { cat "$builddir"/meson-logs/meson-log.txt; return 1; }
}

# Core, no specified extras or compilers
msetup

# Unversioned compilers
msetup gcc
msetup clang

# Versioned compilers
msetup gcc8
msetup gcc9
msetup gcc10
msetup gcc11
msetup gcc12
msetup clang9
msetup clang10
msetup clang11
msetup clang12
msetup clang13
msetup clang14
msetup clang15

# Vendor-provided extras
if [ -d /usr/local/cuda ]; then
  msetup -Dcuda=enabled
fi
if [ -d /opt/rocm ]; then
  msetup -Drocm=enabled
fi
if [ -d /usr/include/level_zero ]; then
  msetup -Dlevel0=enabled
  if [ -d /opt/gtpin-3.4/Profilers ]; then
    msetup -Dlevel0=enabled -Dgtpin=enabled
  fi
fi
