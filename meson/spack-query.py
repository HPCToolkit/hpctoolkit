from __future__ import print_function
import sys
import shlex
import os.path
import spack

db = spack.store.store.db

def flatten(x):
  o = []
  for e in x:
    if type(e) == list:
      o.extend(flatten(e))
    else:
      o.append(e)
  return o

messaged_for = set()
def message_once(package, message):
  if package in messaged_for:
    return []
  messaged_for.add(package)
  return message

def query(spackpath, mode, package, force_spec, user_spec):
  if len(force_spec) > 0:
    force_spec = ' '+force_spec
  if len(user_spec) > 0:
    user_spec = ' '+user_spec
  assert(mode == '--unique' or mode == '--any')
  out = []

  # Get the list of all the specs that match our arguments
  specs = db.query(package + force_spec + user_spec)
  if len(specs) == 0:
    # There are no matching specs. So try with just the package name.
    specs = db.query(package + force_spec)
    if len(specs) > 0:
      # The package is installed, but the spec is wrong. Report this.
      return message_once(package, ['F'+'''
  No package matching `{0}{1}{2}\' but {0} is installed. Suggestions:
    1. Adjust {0}_spec to match an installed spec (see `{3} find {0}{1}`)
    2. Install the specific spec (`{3} install {0}{1}{2}`)'''
        .format(package, force_spec, user_spec, spackpath)])
    else:
      # The package isn't even installed. Report this.
      return message_once(package, ['E'+'''
  No valid variant of package {0} is installed. Suggestions:
    1. Install the requested spec (`{3} install {0}{1}{2}`)
    2. Set {0}_spec to \'system\' to search the system and current environment
    3. Set the relevant *_root property to an installation for {0}.'''
        .format(package, force_spec, user_spec, spackpath)])
  elif len(specs) > 1:
    # There are multiple matching packages. Report this.
    out = message_once(package, [('F' if mode == '--unique' else 'W')+'''
  Multiple packages match `{0}{1}{2}\'. Suggestions:
    1. Adjust {0}_spec to be more specific (usually `/<hash>`, see `{3} find {0}{1}{2}`)
    2. Remove the conficting packages (`{3} uninstall {0}{1}{2}`)'''
      .format(package, force_spec, user_spec, spackpath)])
    if mode == '--unique':
      return out
  spec = specs[0]

  # Build our summary string
  summary = spec.format('{name}{@version} /{hash:7}')
  if len(specs) > 1:
    summary += ' (multiple match)'

  # Parse each fragment of the environment modifications, clean and save them
  shellcode = spack.user_environment.environment_modifications_for_spec(spec).shell_modifications()
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

  # Compose everything together into our final output.
  return flatten([out, [
    'V'+summary,
    [['I'+x for x in environs.get(env, [])]
     for env in ['CPATH', 'CPLUS_INCLUDE_PATH', 'SPACK_INCLUDE_DIRS']],
    ['I'+x for x in spec_includes],
    [['L'+x for x in environs.get(env, [])]
     for env in ['LIBRARY_PATH', 'SPACK_LINK_DIRS']],
    ['L'+x for x in spec_links],
  ]])

# Process every query, and output stanzas terminated by a |
assert(len(sys.argv) % 5 == 2)
spackpath = sys.argv[2]
queries = sys.argv[2:]
out = []
for i in range(2, len(sys.argv), 4):
  out.extend(query(spackpath, *sys.argv[i:i+4]))
  out.append('|')
print('\0'.join(out))
