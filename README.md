# HPCToolkit

HPCToolkit is an integrated suite of tools for measurement and
analysis of program performance on computers ranging from multicore
desktop systems to the nation's largest supercomputers. HPCToolkit
provides accurate measurements of a program's work, resource
consumption, and inefficiency, correlates these metrics with the
program's source code, works with multilingual, fully optimized
binaries, has very low measurement overhead, and scales to large
parallel systems. HPCToolkit's measurements provide support for
analyzing a program execution cost, inefficiency, and scaling
characteristics both within and across nodes of a parallel system.

HPCToolkit is released under the 3-clause BSD license.
See the file LICENSE for details.

## Getting HPCToolkit via Spack (recommended for users)

HPCToolkit is available through [Spack], simply run:

```console
$ spack install hpctoolkit [+features...]
$ spack load hpctoolkit [+features...]
```

See the [Spack User's Manual] for instructions on how to set up Spack for your system and use it to install packages including HPCToolkit.

Available optional features:

- `+mpi`: Build `hpcprof-mpi` in addition to `hpcprof`. Requires MPI (see next section).
- `+python`: Enable the (experimental) Python unwinder. Requires Python (see next section).
- `+papi`: Enable PAPI metrics. Requires PAPI (see next section).
- `+cuda`: Enable CUDA metrics (`-e gpu=nvidia`). Requires CUDA (see next section).
- `+level0`: Enable Level Zero metrics (`-e gpu=level0`). Requires Level Zero (see next section).
- `+gtpin`: Enable Level Zero instrumentation metrics (`-e gpu=level0,inst`). Requires IGC and GTPin (see next section).
- `+opencl`: Enable OpenCL metrics (`-e gpu=opencl`). Requires OpenCL (see next section).
- `+rocm`: Enable ROCm metrics (`-e gpu=amd`). Requires ROCm (see next section).

### Providing system dependencies

Spack will build optional dependencies from source, however in most cases this is unwanted and unwarranted since applications already use these dependencies. In these cases, Spack can be configured to use these external installations as "external packages" in the `packages.yaml`
configuration file (usually under `$HOME/.spack/packages.yaml`).
For example, the configuration for a system ROCm 5.4.3 installation is:

```yaml
# .../packages.yaml
packages:
  hip:
    buildable: false
    externals:
    - spec: hip@5.4.3
      prefix: /opt/rocm-5.4.3

  hsa-rocr-dev:
    buildable: false
    externals:
    - spec: hsa-rocr-dev@5.4.3
      prefix: /opt/rocm-5.4.3

  roctracer-dev:
    buildable: false
    externals:
    - spec: roctracer-dev@5.4.3
      prefix: /opt/rocm-5.4.3

  rocprofiler-dev:
    buildable: false
    externals:
    - spec: rocprofiler-dev@5.4.3
      prefix: /opt/rocm-5.4.3
```

For more details see the [Spack User's Manual](https://spack.readthedocs.io/en/latest/build_settings.html#external-packages).

## Building HPCToolkit from source with Meson (recommended for developers)

HPCToolkit supports building from source using [Meson]. The prerequisites in this case are:

- Python 3.8 (or higher),
- [Spack][getting spack] 0.20.1 (or higher) configured for the working system, and
- [Meson][getting meson] 1.1.0 (or higher).

Once the prerequisites are installed, the build of HPCToolkit (and core prerequisites) can be done via the standard Meson build sequence:

```console
$ meson setup --prefix=/path/to/prefix \
    -Dspack-deps:spack_mode=install -Dautotools:spack_mode=install \
    [options...] builddir/
$ cd builddir/
$ meson compile  # -OR- ninja
$ meson install  # -OR- ninja install
```

Note that the first `meson setup` may take some time, it will run `spack install` to install the core dependencies.

Available configuration options:

- `-Dhpcprof_mpi=(disabled|auto|enabled)`: Build `hpcprof-mpi` in addition to `hpcprof`. Requires MPI.
- `-Dpython=(disabled|auto|enabled)`: Enable the (experimental) Python unwinder. Requires Python 3.10+.
- `-Dpapi=(disabled|auto|enabled)`: Enable PAPI metrics. Requires PAPI.
- `-Dcuda=(disabled|auto|enabled)`: Enable CUDA metrics (`-e gpu=nvidia`). Requires CUDA.
- `-Dlevel0=(disabled|auto|enabled)`: Enable Level Zero metrics (`-e gpu=level0`). Requires Level Zero.
- `-Dgtpin=(disabled|auto|enabled)`: Also enable Level Zero instrumentation metrics (`-e gpu=level0,inst`). Requires Level Zero, IGC and GTPin.
- `-Dopencl=(disabled|auto|enabled)`: Enable OpenCL metrics (`-e gpu=opencl`). Requires OpenCL.
- `-Drocm=(disabled|auto|enabled)`: Enable ROCm metrics (`-e gpu=amd`). Requires ROCm.
- `-Dvalgrind_annotations=(false|true)`: Inject annotations for debugging with Valgrind.

Note that many of the features above require additional dependencies that will not be installed by Spack. See [Providing and customizing dependencies](#providing-and-customizing-dependencies) below for instructions on how to provide these dependencies.

### Deviances from typical Meson projects

HPCToolkit's Meson support is a work-in-progress, the actual build mixes build systems between Meson and Autotools. This has consequences for developers used to the conveniences of Meson.
Until this is resolved, keep the following caveats in mind:

- The provided regression test suite accessible via `meson test` (or `ninja test`) tests the *installed* software, not the *built* software.
  Before running `meson test` and after making changes, you must manually `meson compile && meson install` (or `ninja && ninja install`).

- The typical `meson compile --clean` (or `ninja clean`) command will only clean Meson-built files, it will not clean the internal Autotools build tree.
  To fully clean the build directory, additionally run `meson compile makeclean` (or `ninja makeclean`).

- Many of [Meson's built-in build options](https://mesonbuild.com/Builtin-options.html) do not affect the Autotools parts of the build.
  For full compatibility, only use the following built-in options that are known to work: `-Dbuildtype`, `-Ddebug`, `-Derrorlogs`, `-Doptimization`, `-Dstdsplit`, `-Dwarning_level`, `-Dwerror`, `-D(c|cpp)_std`, `-D(c|cpp)_args`, `-D(c|cpp)_link_args`.

- Most dependencies cannot be provided via the options that work for most other Meson projects, such as `pkg_config_path` and `cmake_prefix_path`.
  See [Providing and customizing dependencies](#providing-and-customizing-dependencies) below for how to customize the dependencies in these cases.

### Providing and customizing dependencies

Overriding or providing optional dependencies needed to compile HPCToolkit is done via [Meson native files].
To start, write a native file listing the install prefix(es) and/or binaries of the dependencies to configure:

```ini
# .../my-native-file.ini
[binaries]
python = '...'

[properties]
prefix_elfutils = '.../elfutils/'
prefix_dyninst = '.../dyninst/'

[built-in options]
c_link_args = ['-Wl,-rpath=.../elfutils/lib', '-Wl,-rpath=.../dyninst/lib']
cpp_link_args = ['-Wl,-rpath=.../elfutils/lib', '-Wl,-rpath=.../dyninst/lib']
```

<details>
<summary>
All configurable native file fields
</summary>

```ini
[binaries]
c = '...'  # C compiler, may also be a list, e.g. ['ccache', '.../my-gcc']
cxx = '...' # C++ compiler, may also be a list, e.g. ['ccache', '.../my-g++']
cuda = '...' # NVCC compiler, may also be a list, e.g. ['ccache', '.../my-nvcc']
mpicxx = '...'  # MPI C++ compiler
spack = '...'  # Spack command used for fallback dependencies
autoreconf = '...'
autoconf = '...'
aclocal = '...'
autoheader = '...'
autom4te = '...'
automake= '...'
libtoolize = '...'
m4 = '...'
make = '...'
python = '...'

[properties]
prefix_boost = '...'
prefix_bzip = '...'  # libbz2
prefix_dyninst = '...'
prefix_elfutils = '...'  # libelf and libdw
prefix_tbb = '...'  # libtbb
prefix_libmonitor = '...'  # https://github.com/hpctoolkit/libmonitor
prefix_libunwind = '...'
prefix_xerces = '...'  # libxerces-c
prefix_lzma = '...'  # liblzma aka xz
prefix_zlib = '...'
prefix_libiberty = '...'
prefix_xed = '...'  # https://github.com/intelxed/xed
prefix_memkind = '...'  # libmemkind
prefix_yaml_cpp = '...'  # libyaml-cpp
prefix_papi = '...'  # libpapi
prefix_perfmon = '...'  # libpfm4
prefix_opencl = '...'  # Headers-only, opencl-c-headers
prefix_cuda = '...'  # https://developer.nvidia.com/cuda-toolkit
prefix_gtpin = '...'  # https://www.intel.com/content/www/us/en/developer/articles/tool/gtpin.html
prefix_igc = '...'  # https://github.com/intel/intel-graphics-compiler
prefix_level0 = '...'  # Part of https://www.oneapi.io/
prefix_rocm_hip = '/opt/rocm-5.4.3/'  # https://github.com/ROCm-Developer-Tools/HIP
prefix_rocm_hsa = '/opt/rocm-5.4.3/'  # https://github.com/RadeonOpenCompute/ROCR-Runtime
prefix_rocm_tracer = '/opt/rocm-5.4.3/'  # https://github.com/ROCm-Developer-Tools/roctracer
prefix_rocm_profiler = '/opt/rocm-5.4.3/'  # https://github.com/ROCm-Developer-Tools/rocprofiler
```

</details>

Then configure your build by passing the file you have just written as a `--native-file`:

```console
$ meson setup --native-file .../my-native-file.ini ...
```

Note that the libraries for any dependencies provided this way must be available at runtime.
Common solutions are to extend `LD_LIBRARY_PATH`, or to pass `-Wl,-rpath` arguments during the link as shown in the example above.

If all dependencies are provided by native files, you may also set `-D<subproject>:spack_mode=no` (the default), which removes the dependency on Spack.
Native files may be saved to `~/.local/share/meson/native` for convenient access across many builds, see the [official documentation][meson native file] for details.

The following sections detail alternative methods to configure dependencies that may be easier under some circumstances.
Note that native files override all other methods.

#### Alternative: Customizing PATH-based dependencies (MPI, Python)

Some dependencies are pulled from the `PATH` by default. Specifically:

- MPI is determined by `mpicxx`, and
- Python is determined by `python` and/or `python3`.

For these dependencies, you may simply alter your `PATH` as needed to point to the wanted installations, then use `meson setup --wipe` to clear Meson's cache:

```console
$ which mpicxx
/path/to/my/mpi/bin/mpicxx
$ which python3
/path/to/my/python/bin/python3
$ meson setup --wipe builddir/
...
```

Meson will then re-search your new `PATH` for any binaries not specified in a [Meson native file] (described above).

#### Alternative: Customizing core dependencies

All core (non-optional) dependencies will fall back to Spack-built software. Normally the build will automatically create a [Spack environment][spack environments] for this purpose, but you may also pass in custom environments to adjust the fallback dependencies.
To start, create a new Spack environment based on the original (note that this should not be under `builddir/` to avoid being deleted by `meson setup --wipe`):

```console
$ mkdir -p /path/to/my-spack-deps-env/
$ cp .../hpctoolkit/subprojects/spack-deps/spack.yaml /path/to/my-spack-deps-env/
$ # Customize /path/to/my-spack-deps-env/spack.yaml as desired
$ spack -D /path/to/my-spack-deps-env/ install
```

See [`subprojects/spack-deps/spack.yaml`] for the list of dependencies that can be configured this way.
Once all the packages in your customized Spack environment have been installed, you can `setup` a build directory to use it with:

```console
$ meson setup \
    -Dspack-deps:spack_mode=find -Dspack-deps:spack_env=/path/to/custom-spack-deps-env \
    ...
```

Any dependencies not provided by a [Meson native file] (described above) will then be fetched from the custom Spack environment.

If desired, you may also customize the fallback Spack environment for the Autotools by replacing `spack-deps` in the above commands with `autotools`.
Be warned, HPCToolkit is very particular about the versions of the Autotools used in the build which limits the range of customization in this case.
However this may be convenient as it allows setting `-Dspack-deps:spack_env` and `-Dautotools:spack_env` to the same custom environment.

## Building HPCToolkit from source with Autotools

**WARNING**: New developers should use the Meson method described above.

This method uses spack to build hpctoolkit's prerequisites and
then uses the traditional autotools `configure && make && make install`
to install hpctoolkit.  This method is primarily for developers who want
to compile hpctoolkit, edit the source code and recompile, etc.  This
method is also useful if you need some configure option that is not
available through spack.

First, use spack to build hpctoolkit's prerequisites as above.  You
can either build some version of hpctoolkit as in  (which will pull
in the prerequisites), or else use `--only dependencies` to avoid
building hpctoolkit itself.

```console
$ spack install --only dependencies hpctoolkit
```

Then, configure and build hpctoolkit as follows.  Hpctoolkit uses
automake and so allows for parallel make.

```console
$ .../configure  \
    --prefix=/path/to/hpctoolkit/install/prefix  \
    --with-spack=/path/to/spack/install_tree/linux-centos9-x86_64/gcc-11.3.1  \
    # ...
$ make -j <num>
$ make install
```

The argument to `--with-spack` should be the directory containing all of
the individual package directories, normally two directories down from
the top-level `install_tree:root` and named by the platform and compiler.
The following are other options that may be useful.  For the full list of options, see
`./configure -h`.

1. `--enable-all-static`: build hpcprof-mpi statically linked for the
   compute nodes.

1. `--enable-develop`: compile with optimization turned off for
   debugging.

1. `--with-package=path`: specify the install prefix for some
   prerequisite package, mostly for developers who want to use a
   custom, non-spack version of some package.

1. `MPICXX=compiler`: specify the MPI C++ compiler for hpcprof-mpi,
   may be a compiler name or full path.

Note: if your spack install tree has multiple versions or variants for
the same package, then `--with-spack` will select the one with the most
recent directory time stamp (and issue a warning).  If this is not what
you want, then you will need to specify the correct version with a
`--with-package` option.

## Documentation

Documentation is available at the HPCToolkit home page:

http://hpctoolkit.org/index.html

In particular, there is a PDF User's Manual at:

http://hpctoolkit.org/manual/HPCToolkit-users-manual.pdf

## Bug Reports and Questions

The best way to contact us for bug reports or questions is via the
mailing list hpctoolkit-forum =at= rice.edu.  The mailman list is
technically members-only, but you don't really need to be a member.
Just send mail to this address and then we can white-list you.

If reporting a problem, be sure to include sufficient information
about your system, what you tried, what went wrong, etc, to allow us
follow your steps.

- type of system, architecture, linux distribution and version,
  compiler version, glibc version.

- hpctoolkit version.

- if a build failure, then the `config.log` file from the build
  directory and the output from `make`.

- if a run failure, then what command you ran and how it failed.

[getting meson]: https://mesonbuild.com/Getting-meson.html
[getting spack]: https://spack.readthedocs.io/en/latest/getting_started.html
[meson]: https://mesonbuild.com/
[meson native file]: https://mesonbuild.com/Native-environments.html
[spack]: https://spack.io/
[spack environments]: https://spack.readthedocs.io/en/latest/environments.html
[spack user's manual]: https://spack.readthedocs.io/
[`subprojects/spack-deps/spack.yaml`]: /subprojects/spack-deps/spack.yaml
