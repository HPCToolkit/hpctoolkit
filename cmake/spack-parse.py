#!/usr/bin/env python3

import subprocess
import sys

# TODO: Once Spack outputs `prefix` in the JSON, use that instead

# Run the Spack internal command and slurp up its output
spack = subprocess.run([sys.argv[1], 'find', '--format', '{prefix}\n'] + sys.argv[2:],
                       capture_output=True, text=True)
if spack.returncode != 0:
  # Spack exits if there are no matching specs. So if we have errors, just blank.
  sys.exit(0)

# Rejigger the lines into something usable. Strip whitespace and remove blanks.
roots = [x.strip() for x in spack.stdout.splitlines()]
roots = [x for x in roots if len(x) > 0]

# If there are multiple matching installations, error. If there are none, exit.
if len(roots) > 1:
  print("Multiple matching installations, please be more specific!",
        file=sys.stderr)
  sys.exit(1)
if len(roots) == 0:
  sys.exit(0)
root = roots[0]

# Otherwise, just print out the root we found
print(root)
