import sys
assert(sys.version_info > (3,0))
spack = sys.argv[1]
package = sys.argv[2]
spec = ' '.join(sys.argv[3:])

import shlex
import spack

db = spack.database.Database(spack.paths.prefix)

# Get the list of all the specs that match our arguments
specs = db.query(package + spec)
if len(specs) == 0:
  # There are no matching specs. So try with just the package name.
  specs = db.query(package)
  if len(specs) == 0:
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
elif len(specs) > 1:
  # There are multiple matching packages. Report this.
  print('''
Multiple packages match `{0}{1}\'. Suggestions:
  1. Adjust {0}_spec to be more specific (usually `/<hash>`, see `{2} find {0}{1}`)
  2. Remove the conficting packages (`{2} uninstall {0}`)'''
    .format(package, ' '.join(spec), spack), file=sys.stderr)
  sys.exit(1)
spec = specs[0]

# Build our summary string
summary = spec.format('{name}{@version} /{hash:7}')

# Parse each fragment of the environment modifications, clean and save them
shellcode = spack.user_environment.environment_modifications_for_spec(spec).shell_modifications()
environs = {}
for var in shlex.split(shellcode, comments=True):
  if var == 'export': continue
  var, value = var.strip(';').split('=', 1)
  values = [x for x in value.split(':') if len(x) > 0]
  if len(values) > 0: environs[var] = values

# Compose everything together into our final output. Meson supports \0 properly,
# so we use that to separate our elements.
print('\0'.join([summary] + ['I'+x for x in environs['CPATH']]
                + ['L'+x for x in environs['LIBRARY_PATH']]))
