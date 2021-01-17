# * BeginRiceCopyright *****************************************************
#
# $HeadURL$
# $Id$
#
# --------------------------------------------------------------------------
# Part of HPCToolkit (hpctoolkit.org)
#
# Information about sources of support for research and development of
# HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
# --------------------------------------------------------------------------
#
# Copyright ((c)) 2002-2021, Rice University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Rice University (RICE) nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# This software is provided by RICE and contributors "as is" and any
# express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular
# purpose are disclaimed. In no event shall RICE or contributors be
# liable for any direct, indirect, incidental, special, exemplary, or
# consequential damages (including, but not limited to, procurement of
# substitute goods or services; loss of use, data, or profits; or
# business interruption) however caused and on any theory of liability,
# whether in contract, strict liability, or tort (including negligence
# or otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.
#
# ******************************************************* EndRiceCopyright *

tool_includes = main_includes + [include_directories('.')]

all_apps = []
all_scripts = []

# Common strings used for generating the launch scripts. Many of these are
# long and copied from config/.
launch_config = configuration_data()
launch_config.set('copyright_hash', '''
#------------------------------------------------------------
# Part of HPCToolkit (hpctoolkit.org)
#------------------------------------------------------------
#
# Copyright (c) 2002-2021, Rice University.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Rice University (RICE) nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# This software is provided by RICE and contributors "as is" and any
# express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular
# purpose are disclaimed. In no event shall RICE or contributors be
# liable for any direct, indirect, incidental, special, exemplary, or
# consequential damages (including, but not limited to, procurement of
# substitute goods or services; loss of use, data, or profits; or
# business interruption) however caused and on any theory of liability,
# whether in contract, strict liability, or tort (including negligence
# or otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.
''')
launch_config.set('launch_early_options', '''
# Early options that every launch script supports.

case "$1" in
    -V | -version | --version )
        cat <<EOF
${prog_name}: A member of HPCToolkit, version $VERSION
git branch:  $git_version
spack spec:  $spack_spec
install dir: $HPCTOOLKIT
EOF
	exit 0
	;;
esac
''')
launch_config.set('export_hpctoolkit', '''
#
# Find the root of the install directory (prefix) and set HPCTOOLKIT.
#
hpc_path_to_root="${hpc_path_to_root:-..}"
hash_file='hash-file'

# Test for valid hash value.
case "$hash_value" in
    ????* ) ;;
    * ) hash_value=no ;;
esac

if test "x$prog_name" = x ; then
    prog_name="$0"
fi

# $1 = candidate directory
# $2 = hash value from configure
valid_hpctoolkit_hash()
{
    hfile="$1/lib/hpctoolkit/${hash_file}"
    config_hash="$2"
    ret=1

    if test -f "$hfile" ; then
	inst_hash=`cat "$hfile"`
	test "x$config_hash" = "x$inst_hash"
	ret=$?
    fi
    return $ret
}

valid_hpctoolkit_dir()
{
    test -d "$1/lib/hpctoolkit"
}

# 1 -- install prefix + hash.  If the install prefix is the same, then
# the script can be anywhere.
found_dir=no

if test "$hash_value" != no ; then
    if valid_hpctoolkit_hash "$prefix" "$hash_value" ; then
	HPCTOOLKIT="$prefix"
	found_dir=yes
    fi
fi

# 2 -- script $0 + hash.  If the launch script is in the bin subdir
# (or libexec), then the install directory can be anywhere.  We expect
# the first two cases to cover almost all valid cases.

if test "$found_dir" = no && test "$hash_value" != no ; then
    script="$0"
    if test -L "$script" ; then
	script=`realpath "$script"`
    fi
    dir=`dirname "$script"`
    dir=`( cd "${dir}/${hpc_path_to_root}" 2>/dev/null && /bin/pwd )`

    if valid_hpctoolkit_hash "$dir" "$hash_value" ; then
	HPCTOOLKIT="$dir"
	found_dir=yes
    fi
fi

# 3 -- HPCTOOLKIT + hash.  If you've moved both the script and the
# install directory, then you can set HPCTOOLKIT.

if test "$found_dir" = no && test "$hash_value" != no ; then
    if valid_hpctoolkit_hash "$HPCTOOLKIT" "$hash_value" ; then
	found_dir=yes
    fi
fi

# 4 -- If can't match the hash value, then try prefix or HPCTOOLKIT.
# If you've moved the script and rerun configure, then configure will
# install a new hash which the script won't have.

if test "$found_dir" = no ; then
    if valid_hpctoolkit_dir "$prefix" ; then
	HPCTOOLKIT="$prefix"
	found_dir=yes
    elif valid_hpctoolkit_dir "$HPCTOOLKIT" ; then
	found_dir=yes
    fi
fi

# Unable to find install prefix.

if test "$found_dir" = no ; then
    cat <<EOF >&2
$prog_name: Unable to find the hpctoolkit install directory.
Set prefix in this script or else HPCTOOLKIT in the environment
and try again.
EOF
    exit 1
fi

export HPCTOOLKIT
''')
_launch_hash_value = run_command(find_program('date', native: true), check: true).stdout().strip()
launch_config.set('launch_script_vars', '''
# If this script can't find its install directory, then set 'prefix'
# here.  This should only be necessary if you've moved both the script
# and the install directory.
#
# Set 'hash_value' to empty to disable matching the hash with the
# install directory.  Note: re-running configure creates a new hash.
# If you copy this script out of its install directory and rerun
# configure, then the hashes won't match.

prefix=''' + '\'' + get_option('prefix') + '\'' + '''
hash_value=''' + '\'' + _launch_hash_value + '\'' + '''

VERSION=''' + '\'' + meson.project_version() + '\'' + '''
git_version='(Git TODO)'
spack_spec='(Spack TODO)'
''')
configure_file(input: '../../meson/hash-file.in', output: 'hash-file',
  configuration: {'HASH': _launch_hash_value},
  install: true, install_dir: get_option('libdir')/meson.project_name())

# These are pretty much exclusively for hpclink
launch_config.set('HPCLINK_CC', ' '.join(meson.get_compiler('c').cmd_array()))
launch_config.set('CFLAGS', '')

launch_config.set('hpclink_extra_wrap_names', get_option('lush-pthreads').enabled()
  ? 'pthread_mutex_lock pthread_mutex_trylock pthread_mutex_unlock'
  + 'pthread_spin_lock pthread_spin_trylock pthread_spin_unlock pthread_spin_destroy'
  + 'pthread_cond_init pthread_cond_destroy pthread_cond_wait'
  + 'pthread_cond_timedwait pthread_cond_signal pthread_cond_broadcast'
  : '')

subdir('hpcstruct')
subdir('hpcprof')
subdir('hpcprof-mpi')
subdir('hpcproftt')
subdir('hpclump')
subdir('hpctracedump')

subdir('hpcserver')

subdir('hpcfnbounds')
subdir('hpcfnbounds2')
subdir('hpcrun')
