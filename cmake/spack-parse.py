#!/usr/bin/env python3

import subprocess
import sys

# TODO: Once Spack outputs `prefix` in the JSON, use that instead

# Run the Spack internal command and slurp up its output
spack = subprocess.run([sys.argv[1], 'find', '--format', '{prefix}\n'] + sys.argv[2:],
                       capture_output=True, text=True)
if spack.returncode != 0:
  # It might be a Spack error, or it might be that there are no matching specs.
  # So try with just the package name and see if the explicit bits are off.
  spack = subprocess.run([sys.argv[1], 'find', '--format', '{prefix}\n', sys.argv[2]],
                         capture_output=True, text=True)
  if spack.returncode == 0:
    # The package is installed, but the spec is wrong. Report this.
    print('''
No package matching `{0}{1}\' but {0} is installed. Suggestions:
  1. Adjust {0}_spec to match an installed spec (see `{2} find {0}`)
  2. Install the specific spec (`{2} install {0}{1}`)'''
      .format(sys.argv[2], sys.argv[3], sys.argv[1]), file=sys.stderr)
    sys.exit(1)
  else:
    # The package isn't even installed. Report this.
    print('''
Package {0} is not installed. Suggestions:
  1. Install the requested spec (`{2} install {0}{1}`)
  2. Set {0}_spec to \'system\' to search the system and current environment
  3. Set the relevant *_root property to an installation for {0}.'''
      .format(sys.argv[2], sys.argv[3], sys.argv[1]), file=sys.stderr)
    sys.exit(0)  # "Soft" error, warn the user but don't force the issue.

# Rejigger the lines into something usable. Strip whitespace and remove blanks.
roots = [x.strip() for x in spack.stdout.splitlines()]
roots = [x for x in roots if len(x) > 0]

if len(roots) > 1:
  # There are multiple matching packages. Report this.
  print('''
Multiple packages match `{0}{1}\'. Suggestions:
  1. Adjust {0}_spec to be more specific (usually `/<hash>`, see `{2} find {0}{1}`)
  2. Remove the conficting packages (`{2} uninstall {0}`)'''
    .format(sys.argv[2], sys.argv[3], sys.argv[1]), file=sys.stderr)
  sys.exit(1)

assert len(roots) > 0
root = roots[0]

# Otherwise, just print out the root we found
print(root)
