# Copyright 2013-2021 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *
import os

class HpctoolkitMeson(MesonPackage):
    """HPCToolkit is an integrated suite of tools for measurement and analysis
    of program performance on computers ranging from multicore desktop systems
    to the nation's largest supercomputers. By using statistical sampling of
    timers and hardware performance counters, HPCToolkit collects accurate
    measurements of a program's work, resource consumption, and inefficiency
    and attributes them to the full calling context in which they occur."""

    homepage = "http://hpctoolkit.org"
    git      = "https://github.com/HPCToolkit/hpctoolkit.git"
    maintainers = ['mwkrentel']

    version('master', branch='new-buildsys', git='https://github.com/blue42u/hpctoolkit.git')

    # Options for MPI and hpcprof-mpi.  We always support profiling
    # MPI applications.  These options add hpcprof-mpi, the MPI
    # version of hpcprof.  Cray and Blue Gene need separate options
    # because an MPI module in packages.yaml doesn't work on these
    # systems.
    variant('cray', default=False,
            description='Build for Cray compute nodes, including '
            'hpcprof-mpi.')

    variant('mpi', default=False,
            description='Build hpcprof-mpi, the MPI version of hpcprof.')

    # We can't build with both PAPI and perfmon for risk of segfault
    # from mismatched header files (unless PAPI installs the perfmon
    # headers).
    variant('papi', default=True,
            description='Use PAPI instead of perfmon for access to '
            'the hardware performance counters.')

    variant('all-static', default=False,
            description='Needed when MPICXX builds static binaries '
            'for the compute nodes.')

    variant('cuda', default=False,
            description='Support CUDA on NVIDIA GPUs (2020.03.01 or later).')

    boost_libs = (
        '+atomic +chrono +date_time +filesystem +system +thread +timer'
        ' +graph +regex +shared +multithreaded visibility=global'
    )

    depends_on('binutils+libiberty', type='link', when='@master:')
    depends_on('binutils+libiberty~nls', type='link', when='@2020.04.00:trunk')
    depends_on('binutils@:2.33.1+libiberty~nls', type='link', when='@:2020.03.99')
    depends_on('boost' + boost_libs)
    depends_on('bzip2+shared', type='link')
    depends_on('dyninst@9.3.2:')
    depends_on('elfutils+bzip2+xz~nls', type='link')
    depends_on('gotcha@1.0.3:')
    depends_on('intel-tbb+shared')
    depends_on('libdwarf')
    depends_on('libmonitor+hpctoolkit')
    depends_on('libunwind@1.4: +xz+pic', when='@2020.09.00:')
    depends_on('libunwind@1.4: +xz', when='@:2020.08.99')
    depends_on('mbedtls+pic')
    depends_on('xerces-c transcoder=iconv')
    depends_on('xz+pic', type='link', when='@2020.09.00:')
    depends_on('xz', type='link', when='@:2020.08.99')
    depends_on('zlib+shared')

    depends_on('cuda', when='+cuda')
    depends_on('intel-xed', when='target=x86_64:')
    depends_on('papi', when='+papi')
    depends_on('libpfm4', when='~papi')
    depends_on('mpi', when='+mpi')

    conflicts('%gcc@:4.7.99', when='^dyninst@10.0.0:',
              msg='hpctoolkit requires gnu gcc 4.8.x or later')

    conflicts('%gcc@:4.99.99', when='@2020.03.01:',
              msg='hpctoolkit requires gnu gcc 5.x or later')

    conflicts('+cuda', when='@2018.0.0:2019.99.99',
              msg='cuda requires 2020.03.01 or later')

    def meson_args(self):
      args = [
        '-Dhpcrun=enabled', '-Dhpclink=enabled', '-Dhpcstruct=enabled',
        '-Dhpcstruct=enabled',
        '-Dcuda-monitoring=%s' % ('enabled' if '+cuda' in self.spec else 'disabled'),
        '-Dmpi=%s' % ('enabled' if '+mpi' in self.spec else 'disabled'),
      ]
      return args
