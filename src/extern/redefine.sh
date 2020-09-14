#!/bin/sh

objcopy="$1"
pc="$2"
pc_dir="$3"
pc_name="$4"
shift ; shift ; shift ; shift

oc_args="--redefine-sym mmap=hpcrun_real_mmap --redefine-sym munmap=hpcrun_real_munmap"

for lib in "$(PKG_CONFIG_PATH="$pc_dir" "$pc" --libs "$pc_name")"; do
  test -n "$1" || exit 1
  test -f "$lib" || exit 1
  "$objcopy" $oc_args "$lib" "$1" || exit $?
  shift
done
