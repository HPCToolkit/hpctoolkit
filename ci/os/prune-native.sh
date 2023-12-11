#!/bin/sh

# Go through the native files we installed and run the appropriate checks for whether this
# image should support the features they each represent.
for native_file in /usr/share/meson/native/*.ini; do
native_name="$(basename "$native_file" .ini)"
case "$native_name" in
gcc*)
  ver="${native_name##gcc}"
  if ! [ -x /usr/bin/gcc"${ver:+-$ver}" ]; then rm "$native_file"; fi
  ;;
clang*)
  ver="${native_name##clang}"
  if ! [ -x /usr/bin/clang"${ver:+-$ver}" ]; then rm "$native_file"; fi
  ;;
cuda.patch)
  if ! [ -d /usr/local/cuda ]; then rm "$native_file"; fi
  ;;
rocm.patch)
  if ! [ -d /opt/rocm ]; then rm "$native_file"; fi
  ;;
level-zero.patch)
  if ! [ -d /usr/include/level_zero ]; then rm "$native_file"; fi
  ;;
gtpin.patch)
  if ! [ -d /opt/gtpin-3.4/Profilers ]; then rm "$native_file"; fi
  ;;
*)
  echo "Missing test for native file: $native_name (from $native_file)" >&2
  exit 1
  ;;
esac
done
