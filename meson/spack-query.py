import sys
assert(sys.version_info > (3,0))
spackpath = sys.argv[1]
mode = sys.argv[2]
package = sys.argv[3]
force_spec = sys.argv[4]
if len(force_spec) > 0:
  force_spec = ' '+force_spec
user_spec = sys.argv[5]
if len(user_spec) > 0:
  user_spec = ' '+user_spec

assert(len(sys.argv) == 6)
assert(mode == '--unique' or mode == '--any')

import shlex
import os.path
import spack

db = spack.store.store.db

# Get the list of all the specs that match our arguments
specs = db.query(package + force_spec + user_spec)
if len(specs) == 0:
  # There are no matching specs. So try with just the package name.
  specs = db.query(package + force_spec)
  if len(specs) > 0:
    # The package is installed, but the spec is wrong. Report this.
    print('''
No package matching `{0}{1}{2}\' but {0} is installed. Suggestions:
  1. Adjust {0}_spec to match an installed spec (see `{3} find {0}{1}`)
  2. Install the specific spec (`{3} install {0}{1}{2}`)'''
      .format(package, force_spec, user_spec, spackpath), file=sys.stderr)
    sys.exit()
  else:
    # The package isn't even installed. Report this.
    print('''
No valid variant of package {0} is installed. Suggestions:
  1. Install the requested spec (`{3} install {0}{1}{2}`)
  2. Set {0}_spec to \'system\' to search the system and current environment
  3. Set the relevant *_root property to an installation for {0}.'''
      .format(package, force_spec, user_spec, spackpath), file=sys.stderr)
    print('SKIP')  # Not a fatal error, so just skip it
    sys.exit()
elif len(specs) > 1:
  # There are multiple matching packages. Report this.
  print(specs, file=sys.stderr)
  print('''
Multiple packages match `{0}{1}{2}\'. Suggestions:
  1. Adjust {0}_spec to be more specific (usually `/<hash>`, see `{3} find {0}{1}{2}`)
  2. Remove the conficting packages (`{3} uninstall {0}{1}{2}`)'''
    .format(package, force_spec, user_spec, spackpath), file=sys.stderr)
  if mode == '--unique':
    sys.exit()
spec = specs[0]

# Build our summary string
summary = spec.format('{name}{@version} /{hash:7}')
if len(specs) > 1:
  summary += ' (multiple match)'

# Parse each fragment of the environment modifications, clean and save them
env = spack.util.environment.EnvironmentModifications()
spack.build_environment.set_build_environment_variables(spec.package, env, True)
shellcode = env.shell_modifications()
environs = {}
for var in shlex.split(shellcode, comments=True):
  if var == 'export': continue
  var, value = var.strip(';').split('=', 1)
  values = [x for x in value.split(':') if len(x) > 0]
  if len(values) > 0: environs[var] = values

# Also get the paths from the spec itself.
spec_includes = spec[package].headers.directories
spec_links = []
try:
  spec_links.extend(spec[package].libs.directories)
except spack.error.NoLibrariesError:
  pass
spec_links.extend([os.path.join(spec.prefix, libdir) for libdir in ('lib', 'lib64')])

# Compose everything together into our final output. Meson supports \0 properly,
# so we use that to separate our elements.
print('\0'.join(['V'+spec.format('{name}{@version} {/hash:7}')]
                + ['I'+x for x in environs.get('CPATH', [])]
                + ['I'+x for x in environs.get('CPLUS_INCLUDE_PATH', [])]
                + ['I'+x for x in environs.get('SPACK_INCLUDE_DIRS', [])]
                + ['I'+x for x in spec_includes]
                + ['L'+x for x in environs.get('LIBRARY_PATH', [])]
                + ['L'+x for x in environs.get('SPACK_LINK_DIRS', [])]
                + ['L'+x for x in spec_links]
               ))
