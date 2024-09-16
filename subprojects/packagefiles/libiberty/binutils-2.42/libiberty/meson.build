# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: MIT

cc = meson.get_compiler('c')

add_project_arguments('-D_GNU_SOURCE', language: 'c')

cfg = configuration_data()
cfg.set('HAVE_SYS_FILE_H', cc.has_header('sys/file.h') ? 1 : false)
cfg.set('HAVE_SYS_PARAM_H', cc.has_header('sys/param.h') ? 1 : false)
cfg.set('HAVE_LIMITS_H', cc.has_header('limits.h') ? 1 : false)
cfg.set('HAVE_STDLIB_H', cc.has_header('stdlib.h') ? 1 : false)
cfg.set('HAVE_MALLOC_H', cc.has_header('malloc.h') ? 1 : false)
cfg.set('HAVE_STRING_H', cc.has_header('string.h') ? 1 : false)
cfg.set('HAVE_UNISTD_H', cc.has_header('unistd.h') ? 1 : false)
cfg.set('HAVE_STRINGS_H', cc.has_header('strings.h') ? 1 : false)
cfg.set('HAVE_SYS_TIME_H', cc.has_header('sys/time.h') ? 1 : false)
cfg.set('HAVE_TIME_H', cc.has_header('time.h') ? 1 : false)
cfg.set('HAVE_SYS_RESOURCE_H', cc.has_header('sys/resource.h') ? 1 : false)
cfg.set('HAVE_SYS_STAT_H', cc.has_header('sys/stat.h') ? 1 : false)
cfg.set('HAVE_SYS_MMAN_H', cc.has_header('sys/mman.h') ? 1 : false)
cfg.set('HAVE_FCNTL_H', cc.has_header('fcntl.h') ? 1 : false)
cfg.set('HAVE_ALLOCA_H', cc.has_header('alloca.h') ? 1 : false)
cfg.set('HAVE_SYS_PSTAT_H', cc.has_header('sys/pstat.h') ? 1 : false)
cfg.set('HAVE_SYS_SYSMP_H', cc.has_header('sys/sysmp.h') ? 1 : false)
cfg.set('HAVE_SYS_SYSINFO_H', cc.has_header('sys/sysinfo.h') ? 1 : false)
cfg.set('HAVE_MACHINE_HAL_SYSINFO_H', cc.has_header('machine/hal_sysinfo.h') ? 1 : false)
cfg.set('HAVE_SYS_TABLE_H', cc.has_header('sys/table.h') ? 1 : false)
cfg.set('HAVE_SYS_SYSCTL_H', cc.has_header('sys/sysctl.h') ? 1 : false)
cfg.set('HAVE_SYS_SYSTEMCFG_H', cc.has_header('sys/systemcfg.h') ? 1 : false)
cfg.set('HAVE_STDINT_H', cc.has_header('stdint.h') ? 1 : false)
cfg.set('HAVE_STDIO_EXT_H', cc.has_header('stdio_ext.h') ? 1 : false)
cfg.set('HAVE_PROCESS_H', cc.has_header('process.h') ? 1 : false)
cfg.set('HAVE_SYS_PRCTL_H', cc.has_header('sys/prctl.h') ? 1 : false)
cfg.set('HAVE_SYS_WAIT_H', cc.has_header('sys/wait.h') ? 1 : false)
cfg.set('HAVE_TIME_H', cc.has_header('time.h') ? 1 : false)

cfg.set('SIZEOF_INT', cc.sizeof('int'))
cfg.set('SIZEOF_LONG', cc.sizeof('long'))
cfg.set('SIZEOF_SIZE_T', cc.sizeof('size_t'))

# Shortened check assuming >=C99
assert(cc.has_type('long long'))
cfg.set('HAVE_LONG_LONG', 1)
cfg.set('SIZEOF_LONG_LONG', cc.sizeof('long long'))

# Shortened check assuming >=C99
cfg.set('UNSIGNED_64BIT_TYPE', 'uint64_t')

# Avoid defining types in the configuration header
assert(cc.has_type('intptr_t', prefix: '#include <stdint.h>'))
assert(cc.has_type('uintptr_t', prefix: '#include <stdint.h>'))
cfg.set('HAVE_UINTPTR_T', 1)
assert(cc.has_type('ssize_t', prefix: '#include <sys/types.h>'))  # POSIX
assert(cc.has_type('pid_t', prefix: '#include <sys/types.h>'))  # POSIX

cfg.set('STDC_HEADERS', 1)
cfg.set('HAVE_SYS_TYPES_H', 1)

foreach func : [
  'asprintf',
  'atexit',
  'basename',
  'bcmp',
  'bcopy',
  'bsearch',
  'bzero',
  'calloc',
  'clock',
  'ffs',
  'getcwd',
  'getpagesize',
  'gettimeofday',
  'index',
  'insque',
  'memchr',
  'memcmp',
  'memcpy',
  'memmem',
  'memmove',
  'mempcpy',
  'memset',
  'mkstemps',
  'putenv',
  'random',
  'rename',
  'rindex',
  'setenv',
  'snprintf',
  'sigsetmask',
  'stpcpy',
  'stpncpy',
  'strcasecmp',
  'strchr',
  'strdup',
  'strncasecmp',
  'strndup',
  'strnlen',
  'strrchr',
  'strstr',
  'strtod',
  'strtol',
  'strtoul',
  'strtoll',
  'strtoull',
  'strverscmp',
  'tmpnam',
  'vasprintf',
  'vfprintf',
  'vprintf',
  'vsnprintf',
  'vsprintf',
  'waitpid',
  'setproctitle',
  '__fsetlocking',
  'canonicalize_file_name',
  'dup3',
  'getrlimit',
  'getrusage',
  'getsysinfo',
  'gettimeofday',
  'on_exit',
  'pipe2',
  'psignal',
  'pstat_getdynamic',
  'pstat_getstatic',
  'realpath',
  'setrlimit',
  'spawnve',
  'spawnvpe',
  'strerror',
  'strsignal',
  'sysconf',
  'sysctl',
  'sysmp',
  'table',
  'times',
  'wait3',
  'wait4',
]
  cfg.set('HAVE_@0@'.format(func.to_upper()), cc.has_function(func) ? 1 : false)
endforeach

foreach var : ['sys_errlist', 'sys_nerr', 'sys_siglist']
  cfg.set(
    'HAVE_@0@'.format(func.to_upper()),
    cc.has_header_symbol('errno.h', var) ? 1 : false,
  )
endforeach

configure_file(output: 'config.h', configuration: cfg)

_srcs = files(
  'alloca.c',
  'argv.c',
  'bsearch_r.c',
  'choose-temp.c',
  'concat.c',
  'cp-demangle.c',
  'cp-demint.c',
  'cplus-dem.c',
  'crc32.c',
  'd-demangle.c',
  'dwarfnames.c',
  'dyn-string.c',
  'fdmatch.c',
  'fibheap.c',
  'filedescriptor.c',
  'filename_cmp.c',
  'floatformat.c',
  'fnmatch.c',
  'fopen_unlocked.c',
  'getopt.c',
  'getopt1.c',
  'getpwd.c',
  'getruntime.c',
  'hashtab.c',
  'hex.c',
  'lbasename.c',
  'lrealpath.c',
  'make-relative-prefix.c',
  'make-temp-file.c',
  'md5.c',
  'objalloc.c',
  'obstack.c',
  'partition.c',
  'pex-common.c',
  'pex-one.c',
  'pexecute.c',
  'physmem.c',
  'regex.c',
  'rust-demangle.c',
  'safe-ctype.c',
  'sha1.c',
  'simple-object-coff.c',
  'simple-object-elf.c',
  'simple-object-mach-o.c',
  'simple-object-xcoff.c',
  'simple-object.c',
  'sort.c',
  'spaces.c',
  'splay-tree.c',
  'stack-limit.c',
  'strerror.c',
  'strsignal.c',
  'timeval-utils.c',
  'unlink-if-ordinary.c',
  'vprintf-support.c',
  'xasprintf.c',
  'xatexit.c',
  'xexit.c',
  'xmalloc.c',
  'xmemdup.c',
  'xstrdup.c',
  'xstrerror.c',
  'xstrndup.c',
  'xvasprintf.c',
)

if host_machine.system() == 'windows'
  _srcs += files('pex-win32.c')
elif host_machine.system() == 'linux'
  _srcs += files('pex-unix.c')
else
  error('Unsupported system')
endif

# libiberty is always compiled statically and never installed
libiberty_lib = static_library(
  'iberty',
  _srcs,
  c_args: '-DHAVE_CONFIG_H',
  include_directories: iface_inc,
  pic: true,
)
libiberty_dep = declare_dependency(
  link_with: libiberty_lib,
  include_directories: iface_inc,
  sources: iface_sources,
)
meson.override_dependency('libiberty', libiberty_dep)
