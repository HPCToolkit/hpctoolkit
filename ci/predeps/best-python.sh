#!/bin/sh

# Guess the best Python available on the PATH
for trial in python3.12 python3.11 python3.10 python3.9 python3.8 python3 python; do
  if command -v "$trial" > /dev/null 2>&1; then
    exec "$trial" "$@"
  fi
done

echo "No suitable Python found!" >&2
exit 127
