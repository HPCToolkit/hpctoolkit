# Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class Hpctoolkit(AutotoolsPackage):
    """HPCToolkit is an integrated suite of tools for measurement and analysis
    of program performance on computers ranging from multicore desktop systems
    to the nation's largest supercomputers. By using statistical sampling of
    timers and hardware performance counters, HPCToolkit collects accurate
    measurements of a program's work, resource consumption, and inefficiency
    and attributes them to the full calling context in which they occur."""

    homepage = "http://hpctoolkit.org"
    git      = "https://github.com/HPCToolkit/hpctoolkit.git"
    maintainers = ['mwkrentel']

    version('master', branch='master')
    version('gpu', branch='gpu')
    version('2019.12.28', commit='b4e1877ff96069fd8ed0fdf0e36283a5b4b62240')
    version('2019.08.14', commit='6ea44ed3f93ede2d0a48937f288a2d41188a277c')
    version('2018.12.28', commit='8dbf0d543171ffa9885344f32f23cc6f7f6e39bc')
    version('2018.11.05', commit='d0c43e39020e67095b1f1d8bb89b75f22b12aee9')

    # Options for MPI and hpcprof-mpi.  We always support profiling
    # MPI applications.  These options add hpcprof-mpi, the MPI
    # version of hpcprof.  Cray and Blue Gene need separate options
    # because an MPI module in packages.yaml doesn't work on these
    # systems.
    variant('cray', default=False,
            description='Build for Cray compute nodes, including '
            'hpcprof-mpi.')

    variant('bgq', default=False,
            description='Build for Blue Gene compute nodes, including '
            'hpcprof-mpi (up to 2019.12.28 only).')

    variant('mpi', default=False,
            description='Build hpcprof-mpi, the MPI version of hpcprof.')

    # We can't build with both PAPI and perfmon for risk of segfault
    # from mismatched header files (unless PAPI installs the perfmon
    # headers).
    variant('papi', default=False,
            description='Use PAPI instead of perfmon for access to '
            'the hardware performance counters.')

    variant('all-static', default=False,
            description='Needed when MPICXX builds static binaries '
            'for the compute nodes.')

    variant('cuda', default=False,
            description='Support CUDA on NVIDIA GPUs (master or gpu branch).')

    boost_libs = (
        '+atomic +chrono +date_time +filesystem +system +thread +timer'
        ' +graph +regex +shared +multithreaded visibility=global'
    )

    depends_on('binutils+libiberty~nls', type='link')
    depends_on('boost' + boost_libs)
    depends_on('bzip2', type='link')
    depends_on('dyninst')
    depends_on('elfutils~nls', type='link')
    depends_on('intel-tbb')
    depends_on('libdwarf')
    depends_on('libmonitor+hpctoolkit')
    depends_on('libmonitor+bgq', when='+bgq')
    depends_on('libunwind@2018.10.0:')
    depends_on('mbedtls+pic')
    depends_on('xerces-c transcoder=iconv')
    depends_on('xz', type='link')
    depends_on('zlib')

    depends_on('cuda', when='+cuda')
    depends_on('intel-xed', when='target=x86_64:')
    depends_on('papi', when='+papi')
    depends_on('libpfm4', when='~papi')
    depends_on('mpi', when='+mpi')

    conflicts('%gcc@:4.7.99', when='^dyninst@10.0.0:',
              msg='hpctoolkit requires gnu gcc 4.8.x or later')

    conflicts('%gcc@:4.99.99', when='@gpu',
              msg='the gpu branch requires gnu gcc 5.x or later')

    conflicts('%gcc@:4.99.99', when='@master',
              msg='the master branch requires gnu gcc 5.x or later')

    conflicts('+cuda', when='@2018.0.0:2019.99.99',
              msg='cuda is only available on the master or gpu branches')

    flag_handler = AutotoolsPackage.build_system_flags

    def configure_args(self):
        spec = self.spec

        args = [
            '--with-binutils=%s'     % spec['binutils'].prefix,
            '--with-boost=%s'        % spec['boost'].prefix,
            '--with-bzip=%s'         % spec['bzip2'].prefix,
            '--with-dyninst=%s'      % spec['dyninst'].prefix,
            '--with-elfutils=%s'     % spec['elfutils'].prefix,
            '--with-tbb=%s'          % spec['intel-tbb'].prefix,
            '--with-libdwarf=%s'     % spec['libdwarf'].prefix,
            '--with-libmonitor=%s'   % spec['libmonitor'].prefix,
            '--with-libunwind=%s'    % spec['libunwind'].prefix,
            '--with-mbedtls=%s'      % spec['mbedtls'].prefix,
            '--with-xerces=%s'       % spec['xerces-c'].prefix,
            '--with-lzma=%s'         % spec['xz'].prefix,
            '--with-zlib=%s'         % spec['zlib'].prefix,
        ]

        if '+cuda' in spec:
            cupti_path = join_path(spec['cuda'].prefix, 'extras', 'CUPTI')
            args.extend([
                '--with-cuda=%s' % spec['cuda'].prefix,
                '--with-cupti=%s' % cupti_path,
            ])

        if spec.target.family == 'x86_64':
            args.append('--with-xed=%s' % spec['intel-xed'].prefix)

        if '+papi' in spec:
            args.append('--with-papi=%s' % spec['papi'].prefix)
        else:
            args.append('--with-perfmon=%s' % spec['libpfm4'].prefix)

        # MPI options for hpcprof-mpi.
        if '+cray' in spec:
            args.extend([
                '--enable-mpi-search=cray',
                '--enable-all-static',
            ])
        elif '+bgq' in spec:
            args.extend([
                '--enable-mpi-search=bgq',
                '--enable-all-static',
                '--enable-bgq',
            ])
        elif '+mpi' in spec:
            args.append('MPICXX=%s' % spec['mpi'].mpicxx)

        if '+all-static' in spec:
            args.append('--enable-all-static')

        return args
