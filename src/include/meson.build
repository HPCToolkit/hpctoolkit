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
# Copyright ((c)) 2002-2020, Rice University
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

hpctoolkit_config.set('USE_SYSTEM_BYTESWAP', false)
if host_machine.endian() == 'little'
  hpctoolkit_config.set('USE_SYSTEM_BYTESWAP',
                        cc.has_header_symbol('byteswap.h', 'bswap_64'))
elif host_machine.endian() == 'big'
  # All good, native format will work for output
else
  error('Unknown endian: ' + host_machine.endian())
endif

hpctoolkit_config.set('BINUTILS_234', libbfd_dep.version().version_compare('>=2.34'))
hpctoolkit_config.set('DATACENTRIC_TRACE', false)  # TODO: Change to get_option
if dyninst_dep.found()
  hpctoolkit_config.set('DYNINST_INSTRUCTION_PTR', cpp.compiles('''
    #include <map>
    #include <iostream>
    #include <Symtab.h>
    #include <Instruction.h>
    #include <CFG.h>
    using namespace Dyninst;
    using namespace InstructionAPI;
    using namespace ParseAPI;
    int main() {
      std::map<Offset, Instruction::Ptr> imap;
      ((Block*)NULL)->getInsns(imap);
      std::cout << imap.begin()->second->size();
    }
  ''', dependencies: dyninst_all_deps))
  hpctoolkit_config.set('DYNINST_USE_CUDA', cpp.compiles('''
    #include <iostream>
    #include <Symtab.h>
    int main() {
      std::cout << (long) Dyninst::Arch_cuda;
    }
  ''', dependencies: dyninst_all_deps))
  # TODO: Change this out for an actual test
  hpctoolkit_config.set('DYNINST_USE_LIBDW', true)
endif

hpctoolkit_config.set('ENABLE_CLOCK_REALTIME', cc.compiles('''
  #include <sys/syscall.h>
  #include <signal.h>
  #include <time.h>
  #include <unistd.h>
  #ifndef sigev_notify_thread_id
  #define sigev_notify_thread_id _sigev_un._tid
  #endif
  int main() {
    struct sigevent sigev;
    timer_t timerid;
    sigev.sigev_notify = SIGEV_THREAD_ID;
    sigev.sigev_signo = SIGRTMIN + 3;
    sigev.sigev_value.sival_ptr = &timerid;
    sigev.sigev_notify_thread_id = syscall(SYS_gettid);
    return timer_create(CLOCK_REALTIME, &sigev, &timerid);
  }
'''))
hpctoolkit_config.set('ENABLE_CLOCK_CPUTIME',
  hpctoolkit_config.get('ENABLE_CLOCK_REALTIME', false) and cc.compiles('''
    #include <sys/time.h>
    #include <time.h>
    int main() {
      clockid_t clock = CLOCK_THREAD_CPUTIME_ID;
      return 0;
    }
  '''))

# TODO: This should depend on a get_option and some tests for Dyninst
hpctoolkit_config.set('ENABLE_OPENMP', true)
hpctoolkit_config.set('ENABLE_OPENMP_SYMTAB', true)

# TODO: This should be set based on a get_option
hpctoolkit_config.set('ENABLE_VG_ANNOTATIONS', false)

# TODO: I've never used this one, maybe nobody should?
hpctoolkit_config.set('ENABLE_XOP', false)

if cuda_dep.found()
  # CUDA is weird, for now just stick with a compile test
  hpctoolkit_config.set('HAVE_CUPTI_H', cpp.has_header('cupti.h', dependencies: cuda_dep))
endif

hpctoolkit_config.set('HAVE_DLFCN_H', cc.has_header('dlfcn.h'))
hpctoolkit_config.set('HAVE_INTTYPES_H', cc.has_header('inttypes.h'))

# TODO: This should be replaced with something more useful
hpctoolkit_config.set('HAVE_OS_MULTILIB', false)

hpctoolkit_config.set('HAVE_STDINT_H', cc.has_header('stdint.h'))
hpctoolkit_config.set('HAVE_SYS_TYPES_H', cc.has_header('sys/types.h'))

hpctoolkit_config.set('HAVE_UINT', cpp.has_type('uint'))
hpctoolkit_config.set('HAVE_UINT_LANG_C', cc.has_type('uint'))
hpctoolkit_config.set('HAVE_ULONG', cpp.has_type('ulong'))
hpctoolkit_config.set('HAVE_ULONG_LANG_C', cc.has_type('ulong'))
hpctoolkit_config.set('HAVE_USHORT', cpp.has_type('ushort'))
hpctoolkit_config.set('HAVE_USHORT_LANG_C', cc.has_type('ushort'))

hpctoolkit_config.set('HOST_BIG_ENDIAN', host_machine.endian() == 'big')
hpctoolkit_config.set('HOST_LITTLE_ENDIAN', host_machine.endian() == 'little')
hpctoolkit_config.set('HOST_CPU_ARM64', host_machine.cpu_family() == 'aarch64')
hpctoolkit_config.set('HOST_CPU_IA64', host_machine.cpu_family() == 'ia64')
hpctoolkit_config.set('HOST_CPU_PPC', host_machine.cpu_family() == 'ppc64')
hpctoolkit_config.set('HOST_CPU_x86', host_machine.cpu_family() == 'x86')
hpctoolkit_config.set('HOST_CPU_x86_64', host_machine.cpu_family() == 'x86_64')
hpctoolkit_config.set('HOST_OS_IRIX', false)  # TODO: What's an IRIX machine?
hpctoolkit_config.set('HOST_OS_LINUX', host_machine.system() == 'linux')
hpctoolkit_config.set('HOST_OS_MAXOS', host_machine.system() == 'darwin')
hpctoolkit_config.set('HOST_OS_SOLARIS', host_machine.system() == 'sunos')
hpctoolkit_config.set('HOST_OS_Tru64', false)  # TODO: What's a Tru64 machine?
hpctoolkit_config.set('HOST_PLATFORM_MIPS64LE_LINUX',
  host_machine.system() == 'linux' and host_machine.cpu_family() == 'mips64')

# TODO: Make this a proper option/test
hpctoolkit_config.set('HOST_SYSTEM_IBM_BLUEGENE', false)

# TODO: Base this on the output from vcs_tag
hpctoolkit_config.set_quoted('HPCTOOLKIT_GIT_VERSION', '(Git TODO)')

# TODO: This really needs to be removed because of DESTDIR
hpctoolkit_config.set_quoted('HPCTOOLKIT_INSTALL_PREFIX', get_option('prefix'))

# TODO: This one is going to be a headache at some point
hpctoolkit_config.set_quoted('HPCTOOLKIT_SPACK_SPEC', '(Spack TODO)')

hpctoolkit_config.set_quoted('HPCTOOLKIT_VERSION', meson.project_version())
hpctoolkit_config.set_quoted('HPCTOOLKIT_VERSION_STRING',
  'A member of ' + meson.project_name() + ', version ' + meson.project_version())

# TODO: Make this a test of some kind, eventually
hpctoolkit_config.set('NO_STD_CHEADERS', false)

hpctoolkit_config.set('OPT_HAVE_CUDA', cuda_dep.found())

hpctoolkit_config.set('SIZEOF_VOIDP', cc.sizeof('void*'))

# TODO: Make this an option instead
hpctoolkit_config.set('USE_LIBUNW', libunwind_dep.found())

hpctoolkit_config.set('USE_ROCM', rocm_headers_dep.found())

configure_file(configuration: hpctoolkit_config, output: 'hpctoolkit-config.h')
main_includes += include_directories('.')
