#!/usr/bin/env python3

import subprocess
import sys
import shlex

assert(3 <= len(sys.argv) and len(sys.argv) <= 4)
spack = sys.argv[1]
package = sys.argv[2]
spec = sys.argv[3:]

# First check that there's exactly one package that matches the spec.
sfind = subprocess.run([spack, 'find', '--format',
                        '{name}{@version} /{hash:7};;;/{hash}\n',
                        package] + spec,
                       capture_output=True, text=True)
if sfind.returncode != 0:
  # It might be a Spack error, or it might be that there are no matching specs.
  # So try with just the package name and see if the explicit spec is off.
  sfind = subprocess.run([spack, 'find', package],
                         capture_output=True, text=True)
  if sfind.returncode == 0:
    # The package is installed, but the spec is wrong. Report this.
    print('''
No package matching `{0}{1}\' but {0} is installed. Suggestions:
  1. Adjust {0}_spec to match an installed spec (see `{2} find {0}`)
  2. Install the specific spec (`{2} install {0}{1}`)'''
      .format(package, ' '.join(spec), spack), file=sys.stderr)
    sys.exit(1)
  else:
    # The package isn't even installed. Report this.
    print('''
Package {0} is not installed. Suggestions:
  1. Install the requested spec (`{2} install {0}{1}`)
  2. Set {0}_spec to \'system\' to search the system and current environment
  3. Set the relevant *_root property to an installation for {0}.'''
      .format(package, ' '.join(spec), spack), file=sys.stderr)
    sys.exit(0)  # "Soft" error, warn the user but don't force the issue.

# Ensure that there is only one non-whitespace line. That's the final "name"
# we'll print for this package.
lines = [x.strip() for x in sfind.stdout.splitlines()]
lines = [x for x in lines if len(x) > 0]
if len(lines) > 1:
  # There are multiple matching packages. Report this.
  print('''
Multiple packages match `{0}{1}\'. Suggestions:
  1. Adjust {0}_spec to be more specific (usually `/<hash>`, see `{2} find {0}{1}`)
  2. Remove the conficting packages (`{2} uninstall {0}`)'''
    .format(package, ' '.join(spec), spack), file=sys.stderr)
  sys.exit(1)
assert len(lines) > 0

# Take the hash off the end of the line so we can ensure we're referencing the
# same install for the next step.
summary, hashspec = lines[0].rsplit(';;;', 1)

# Run Spack again, this time for the proper include and library paths.
sload = subprocess.run([spack, 'load', '--sh', '--only=package', hashspec],
                       capture_output=True, text=True)
if sload.returncode != 0:
  # This shouldn't happen.
  print('Failed to {0} load {1}, something went horribly wrong!'
    .format(spack, hashspec), file=sys.stderr)
  sys.exit(1)

# Parse each fragment of the resulting output, and clean and save them.
environs = {}
for var in shlex.split(sload.stdout, comments=True):
  if var == 'export': continue
  var, value = var.strip(';').split('=', 1)
  values = [x for x in value.split(':') if len(x) > 0]
  if len(values) > 0: environs[var] = values

# Compose everything together into our final output. Meson supports \0 properly,
# so we use that to separate our elements.
print('\0'.join([summary] + ['I'+x for x in environs['CPATH']]
                + ['L'+x for x in environs['LIBRARY_PATH']]))
