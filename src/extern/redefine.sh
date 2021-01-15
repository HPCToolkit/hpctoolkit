#!/bin/sh

objcopy="$1"
pc="$2"
pc_dir="$3"
pc_name="$4"
shift 4

oc_args="--redefine-sym mmap=hpcrun_real_mmap"
oc_args="$oc_args --redefine-sym munmap=hpcrun_real_munmap"
oc_args="$oc_args --redefine-sym dl_iterate_phdr=hpcrun_real_dl_iterate_phdr"

for lib in "$(PKG_CONFIG_PATH="$pc_dir" "$pc" --libs "$pc_name")"; do
  if [ -z "$1" ]; then echo "missing output filename: $lib" >&2; exit 1; fi
  if [ ! -f "$lib" ]; then echo "file does not exist: $lib" >&2; exit 1; fi
  "$objcopy" $oc_args "$lib" "$1" || exit $?
  shift
done
