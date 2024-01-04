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
$ meson test     # -OR- ninja test
$ meson install  # -OR- ninja install
```

Note that the first `meson setup` may take some time, it will run `spack install` to install the core dependencies.
If this is unwanted, set the `spack_mode` options above to `find` to avoid installing any new packages or to `no` to disallow using Spack altogether, see [Providing dependencies from Spack](#alternative-providing-dependencies-from-spack) below for details.
Note that doing this may require additional configuration to [provide core dependencies](#providing-and-customizing-dependencies) before the build will work.

Available configuration options:

- `-Dhpcprof_mpi=(disabled|auto|enabled)`: Build `hpcprof-mpi` in addition to `hpcprof`. Requires MPI.
- `-Dpython=(disabled|auto|enabled)`: Enable the (experimental) Python unwinder.
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

- The typical `meson compile --clean` (or `ninja clean`) command will only clean Meson-built files, it will not clean the internal Autotools build tree.
  To fully clean the build directory, additionally run `meson compile makeclean` (or `ninja makeclean`).

- Many of [Meson's built-in build options](https://mesonbuild.com/Builtin-options.html) do not affect the Autotools parts of the build.
  For full compatibility, only use the following built-in options that are known to work: `-Dbuildtype`, `-Ddebug`, `-Derrorlogs`, `-Doptimization`, `-Dstdsplit`, `-Dwarning_level`, `-Dwerror`, `-D(c|cpp)_std`, `-D(c|cpp)_args`, `-D(c|cpp)_link_args`.

- Most dependencies cannot be provided via the options that work for most other Meson projects, such as `pkg_config_path` and `cmake_prefix_path`.
  See [Providing and customizing dependencies](#providing-and-customizing-dependencies) below for how to customize the dependencies in these cases.

### Providing and customizing dependencies

Overriding or providing optional dependencies needed to compile HPCToolkit is done via Meson "native" files.
For a more in-depth description of how these files are written and stored, please read the official documentation on [native files][meson native file] and [machine files in general][meson machine file].
The instructions below may be read before reading these documents but some parts may be confusing.

In HPCToolkit, most library dependencies are defined by their install prefix as machine properties.
These properties are taken from native files, such as the example below:

```ini
# …/my-elf-dyn.ini
[properties]
prefix_elfutils = '/…/path/to/elfutils/install/'
prefix_dyninst = '/…/path/to/dyninst/install/'

# If you don't extend `LD_LIBRARY_PATH`, you will also need to specify rpaths
# for custom install prefixes so libraries can be found at runtime.
# The necessary compiler flags can be specified in the Meson configuration:
#     meson (setup|configure) -Dc_link_args=… -Dcpp_link_args=… [etc…]
# Or, directly in the native file as shown below.
#
# Due to a Meson bug (https://github.com/mesonbuild/meson/issues/11930), it is
# highly recommended to use either `-D` flags or this section BUT NOT BOTH.
[constants]
rpath_args = [
  '-Wl,-rpath=/…/path/to/elfutils/install/lib',
  '-Wl,-rpath=/…/path/to/dyninst/install/lib'
  ]
[built-in options]
c_link_args = rpath_args
cpp_link_args = rpath_args
```

Build tools and some library dependencies are defined by the path to the executable, again written in native files:

```ini
# …/my-mpi-python.ini
[binaries]
mpicxx = '/…/path/to/mpi/install/bin/mpicxx'
# You can also specify just the executable's name if it's already on your PATH:
python = 'python3.12'
```

A complete template native file is available [here](/doc/developers/meson.ini).
The configuration set by native files is applied by passing a `--native-file` argument when setting up a build directory:

```console
$ meson setup --native-file …/my-elf-dyn.ini --native-file …/my-mpi-python.ini ...
```

Note that Meson will track the paths to native files and automatically reconfigure whenever they change.
This means you should not delete or move native files that are in use, but nor do you need to `meson setup` every time you adjust an installation prefix or build tool.

Native files are the preferred way to provide or customize dependencies, and take precedence over all alternatives (other than `meson (setup|configure) -D` options).
For very specific use cases, there are alternatives available that may be more convenient than writing a native file.
These are described in the subsections below.

#### Alternative: Providing dependencies from the PATH

Meson will by default respect some dependencies you have "loaded" into your shell environment, e.g. by:

```console
$ export PATH=/…/path/to/bin/:$PATH  # -OR-
$ module load python mpi  # -OR-
$ spack load python mpi # -OR-
$ # etc.
```

Currently this only works for MPI (`mpicxx`) and Python (`python` or `python3`).
If one of these dependencies is not provided by a native file as described above, it will be pulled from the `PATH` when Meson (re-)configures the build directory.

Note that if you load a different package (e.g. `module swap python/3.10 python/3.12`) Meson will still be using the cached data from the package it saw first.
You can clear this cache using the `meson configure --clearcache` or `meson setup --wipe` options.

#### Alternative: Providing dependencies from Spack

We have custom support in HPCToolkit for pulling missing core dependencies from Spack.
How this happens is controlled by the `spack_mode` option for the `spack-deps` and `autotools` subprojects:

- `-D(spack-deps|autotools):spack_mode=no`: Don't use Spack for missing core dependencies. (Default)
- `-D(spack-deps|autotools):spack_mode=find`: Use only prebuilt Spack packages.
- `-D(spack-deps|autotools):spack_mode=install`: Install any missing Spack packages if they aren't built already.

The `autotools` subproject provides fallbacks for the Autotools (`automake`, `autoconf`, `libtool`, etc.) while the `spack-deps` subproject provides fallbacks for other core dependencies.
By default these subprojects will automatically create (and if allowed, install) a [Spack environment][spack environments] in the build directory with all the dependencies.
You can customize this environment by setting the `-D(spack-deps|autotools):spack_env` option to a Spack environment directory of your choosing:

```console
$ spack env create my-hpct-deps …/hpctoolkit/subprojects/spack-deps/spack.yaml
$ spack -e my-hpct-deps config edit  # Customize the environment as appropriate
$ spack -e my-hpct-deps install
$ meson configure \
>   -Dspack-deps:spack_env=$(spack location -e my-hpct-deps) -Dspack-deps:spack_mode=find
```

You are free to add arbitrary packages to this environment, but they will NOT be used by the build.
Only packages listed in [`subprojects/spack-deps/spack.yaml`] will be used as fallback dependencies.
Use a native file as described above to provide/customize other dependencies.

## Building HPCToolkit from source with Autotools

**WARNING**: New developers should use the Meson method described above. This text is only kept for legacy reasons.

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
[meson machine file]: https://mesonbuild.com/Machine-files.html
[meson native file]: https://mesonbuild.com/Native-environments.html
[spack]: https://spack.io/
[spack environments]: https://spack.readthedocs.io/en/latest/environments.html
[spack user's manual]: https://spack.readthedocs.io/
[`subprojects/spack-deps/spack.yaml`]: /subprojects/spack-deps/spack.yaml
