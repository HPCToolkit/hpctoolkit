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

HPCToolkit supports building from source using [Meson].

### Quickstart

Install:

- [Meson] >=1.3.2 and a working C/C++ compiler (GCC, Clang, etc.)
- [Boost](https://www.boost.org/) >=1.71.0, if using CUDA support this must also be built with `visibility=global` (see [#815](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/815))
- [libmonitor](https://github.com/hpctoolkit/libmonitor) configured with `--disable-dlfcn --enable-client-signals='SIGBUS, SIGSEGV, SIGPROF, 36, 37, 38'` (also see [#793](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/793))
- `make`, `awk`, and `sed` for `hpcstruct` measurement directory support (see [#704](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/704)).
- (Optional but highly recommended:) [ccache](https://ccache.dev/)
- (Optional:) [Wrapped or additional prerequisites](#prerequisites)

Then run:

```console
$ meson setup builddir/
$ cd builddir/
$ meson compile  # -OR- ninja
$ meson test     # -OR- ninja test
```

**Do not `meson install`/`ninja install` here.**
Instead, working versions of the tools themselves are available under the `meson devenv`, either as individual commands or as a whole subshell environment:

```console
$ meson devenv hpcrun --version  # -OR-
$ meson devenv
[builddir] $ hpcrun --version
```

### Configuration

Additional configuration arguments can be passed to the initial `meson setup` or later `meson configure` invocations:

- `-Dmanpages=(disabled|auto|enabled)`: Generate and build man pages. Requires [Docutils](https://www.docutils.org/).
- `-Dhpcprof_mpi=(disabled|auto|enabled)`: Build `hpcprof-mpi` in addition to `hpcprof`. Requires MPI.
- `-Dpython=(disabled|auto|enabled)`: Enable the (experimental) Python unwinder. Requires Python headers.
- `-Dpapi=(disabled|auto|enabled)`: Enable PAPI metrics. Requires PAPI.
- `-Dcuda=(disabled|auto|enabled)`: Enable CUDA metrics (`-e gpu=nvidia`). Requires CUDA.
- `-Dlevel0=(disabled|auto|enabled)`: Enable Level Zero metrics (`-e gpu=level0`). Requires Level Zero.
- `-Dgtpin=(disabled|auto|enabled)`: Also enable Level Zero instrumentation metrics (`-e gpu=level0,inst`). Requires Level Zero, IGC/IGA and GTPin.
- `-Dopencl=(disabled|auto|enabled)`: Enable OpenCL metrics (`-e gpu=opencl`). Requires OpenCL headers.
- `-Drocm=(disabled|auto|enabled)`: Enable ROCm metrics (`-e gpu=amd`). Requires ROCm.
- `-Dvalgrind_annotations=(false|true)`: Inject annotations for debugging with Valgrind.
- `-Dextended_tests=(disabled|auto|enabled)`: Feature option for that require extra dependencies. Request:
  - CUDA compiler (e.g. `nvcc`) if `-Dcuda` support is available/enabled
  - HIP compiler (`hipcc`) if `-Drocm` support is available/enabled
  - SYCL-compatible compiler (e.g. `icpx`) if `-Dlevel0` support is available/enabled

Note that many of the features above require additional optional dependencies when enabled.

### Prerequisites

#### Debian/Ubuntu and derivatives

For Debian Sid/Ubuntu 23.10:

```bash
sudo apt-get install git build-essential ccache ninja-build meson cmake pkg-config python3-venv libboost-all-dev libbz2-dev libtbb-dev libelf-dev libdw-dev libunwind-dev libxerces-c-dev libiberty-dev libyaml-cpp-dev libpfm4-dev libxxhash-dev zlib1g-dev
pipx install 'meson>=1.3.2'  # Optional but recommended
```

For older versions:

```bash
sudo apt-get install git build-essential ccache ninja-build cmake pkg-config pipx python3 python3-pip python3-venv libboost-all-dev libbz2-dev libtbb-dev libelf-dev libdw-dev libunwind-dev libxerces-c-dev libiberty-dev libyaml-cpp-dev libpfm4-dev libxxhash-dev zlib1g-dev
pipx install 'meson>=1.3.2'
```

Note that some dependencies may be [built from wraps](#wraps-no-root-required) by default, either because they aren't packaged in Debian/Ubuntu (e.g. Dyninst) or because the version is too old.
Additional packages can be installed for optional features:

| Package | Feature option | Notes |
| --- | --- | --- |
| `mpi-default-dev` | `-Dhpcprof_mpi=enabled` |
| `libpapi-dev` | `-Dpapi=enabled` |
| `nvidia-cuda-toolkit` | `-Dcuda=enabled` |
| [`level-zero-dev`](https://dgpu-docs.intel.com/driver/installation.html#ubuntu-install-steps) | `-Dlevel0=enabled` |
| `libigdfcl-dev` | `-Dlevel0=enabled` |
| `libigc-dev` | `-Dgtpin=enabled` |
| `opencl-headers` | `-Dopencl=enabled` | [Optional, available as wrap](#wraps-no-root-required) |
| [`rocm-hip-libraries`](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/index.html) | `-Drocm=enabled` |

#### Fedora/RHEL and derivatives

For RHEL 8:

```bash
sudo dnf install git gcc gcc-c++ ccache ninja-build cmake pkg-config python39 python39-pip boost-devel bzip2-devel tbb-devel elfutils-devel xerces-c-devel binutils-devel yaml-cpp-devel libpfm-devel xxhash-devel zlib-devel
python3.9 -m pip install pipx
pipx install 'meson>=1.3.2'
```

For RHEL 9:

```bash
sudo dnf install git gcc gcc-c++ ccache ninja-build cmake pkg-config python3 pipx boost-devel bzip2-devel tbb-devel elfutils-devel xerces-c-devel binutils-devel yaml-cpp-devel libpfm-devel libunwind-devel xxhash-devel zlib-devel
pipx install 'meson>=1.3.2'
```

For Fedora:

```bash
sudo dnf install git gcc gcc-c++ ccache ninja-build cmake pkg-config python3 meson pipx boost-devel bzip2-devel tbb-devel elfutils-devel xerces-c-devel binutils-devel yaml-cpp-devel libpfm-devel libunwind-devel xxhash-devel zlib-devel
pipx install 'meson>=1.3.2'  # Optional but recommended
```

Note that some dependencies may be [built from wraps](#wraps-no-root-required) by default, either because they aren't packaged in Fedora/RHEL (e.g. Xed) or because the version is too old.
Additional packages can be installed for optional features:

| Package | Feature option | Notes |
| --- | --- | --- |
| `openmpi-devel` or `mpich-devel` or `mvapich2-devel` | `-Dhpcprof_mpi=enabled` |
| `papi-devel` | `-Dpapi=enabled` |
| `cuda-toolkit` | `-Dcuda=enabled` |
| [`level-zero-devel`](https://dgpu-docs.intel.com/driver/installation.html#red-hat-enterprise-linux-install-steps) | `-Dlevel0=enabled` |
| `intel-igc-opencl-devel` | `-Dlevel0=enabled` |
| `opencl-headers` | `-Dopencl=enabled` | [Optional, available as wrap](#wraps-no-root-required) |
| [`rocm`](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/index.html) | `-Drocm=enabled` |

#### SUSE Leap/SLES 15 and derivatives

```bash
sudo zypper install git gcc12 gcc12-c++ ccache ninja-build cmake pkg-config python311 python311-pip libboost_atomic-devel libboost_chrono-devel libboost_date_time-devel libboost_filesystem-devel libboost_graph-devel libboost_system-devel libboost_thread-devel libboost_timer-devel libbz2-devel tbb-devel libxerces-c-devel binutils-devel yaml-cpp-devel libpfm-devel xxhash-devel zlib-devel
python3.11 -m pip install pipx
pipx install 'meson>=1.3.2'
```

Note that some dependencies may be [built from wraps](#wraps-no-root-required) by default, either because they aren't packaged in SUSE Leap (e.g. Xed) or because the version is too old.
Additional packages can be installed for optional features:

| Package | Feature option | Notes |
| --- | --- | --- |
| `openmpi-devel` or `mpich-devel` | `-Dhpcprof_mpi=enabled` |
| `papi-devel` | `-Dpapi=enabled` |
| `cuda-toolkit` | `-Dcuda=enabled` |
| [`level-zero-devel`](https://dgpu-docs.intel.com/driver/installation.html#red-hat-enterprise-linux-install-steps) | `-Dlevel0=enabled` |
| `intel-igc-opencl-devel` | `-Dlevel0=enabled` |
| `opencl-headers` | `-Dopencl=enabled` | [Optional, available as wrap](#wraps-no-root-required) |
| [`rocm`](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/index.html) | `-Drocm=enabled` |

#### Wraps (no root required)

HPCToolkit provides [Meson wraps] for most core dependencies, by default these wraps act as a fallback if they cannot be found otherwise.
The convenient part about this is that building HPCToolkit requires little more than Meson and a C/C++ compiler, any missing dependencies will be downloaded and built as subprojects:

```
  Subprojects
    bzip2         : YES
    dyninst       : YES 347 warnings
    elfutils      : YES 2 warnings
    libiberty     : YES (from dyninst)
    liblzma       : YES (from elfutils)
    libpfm        : YES
    libunwind     : YES
    opencl-headers: YES
    tbb           : YES
    xed           : YES
    xerces-c      : YES
    xxhash        : YES
    yaml-cpp      : YES
    zstd          : YES (from elfutils)
```

Note that dependencies that are not required will not use a wrap by default, you may need to enable a feature before a wrap will be used (e.g. `-Dopencl=enabled`).
If the internet is not available on the system running `meson setup`, you may also need to run `meson subprojects download` first on a system with internet to download the required sources.

If wraps are not desired, this fallback behavior can be disabled with `meson (setup|configure) --wrap-mode=nofallback`.
With this, all dependencies must be installed or otherwise loaded into the build environment, except for force-fallback dependencies `meson (setup|configure) --force-fallback-for=dep1,dep2,...`.

See the official Meson documentation for [Meson subprojects] and [wraps][meson wraps] for more configuration options and implementation details.

#### Dev Containers (BETA, no root required)

Multiple [Dev Container](https://containers.dev/) configurations are provided for common distros and vendor software:

- `ubuntu20.04`, `rhel8`, `leap15`, `fedora39`: CPU-only configurations for common distros
- `cuda*`: Ubuntu 20.04 with NVIDIA CUDA toolkit installed
- `rocm*`: Ubuntu 20.04 with AMD ROCm installed
- `intel`: Ubuntu 20.04 with Intel OneAPI Base Toolkit
- `bare`: Bare minimum dependencies to build HPCToolkit

#### Custom Dependencies

It is also possible to use custom versions of dependencies, exposing them to Meson however requires some initial understanding of how Meson finds dependencies.
This section is intended as a "peek behind the curtain" of how Meson understands dependencies, and an abridged version of [Meson's `dependency()` docs] for developers.

Meson fetches all dependencies from the "build environment" aka the "machine" (technically two, the `build_machine` and the `host_machine`, which are only different when [cross-compiling](https://mesonbuild.com/Cross-compilation.html)).
Whenever Meson configures (i.e. during `meson setup` or automatically during `meson compile/install/test/etc.`), queries the "machine" as defined by various environment variables:

- Finding programs (`find_program()`) is done by searching the `PATH`,
- The C/C++ compilers (`CC`/`CXX`), which are also affected by `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `INCLUDE_PATH`, `LIBRARY_PATH`, `CRAY_*`, ...
- Dependencies are found via `pkg-config`, which is affected by `PKG_CONFIG_PATH` (and other `PKG_CONFIG_*` variables),
- Dependencies are found via CMake, which is affected by `CMAKE_PREFIX_PATH` (and other `CMAKE_*`, `*_ROOT` and `*_ROOT_DIR` variables),
- Dependencies with Meson built-in functionality have their own quirks:
  - Boost is found under `BOOST_ROOT`,
  - The CUDA Toolkit is found under `CUDA_PATH`.

These variables are frequently altered by commands designed to load software into a shell environment, for example `module un/load`.
Meson assumes the "machine" does not change between `meson` invocations, however if it does by running the above commands it is possible Meson's cache will not match reality.
In these cases reconfiguration or later compilation may fail due to the inconsistent state between dependencies.
In some cases this can be fixed by clearing Meson's cache (`meson configure --clearcache`), Meson will efficiently avoid rebuilding files if the compile command did not actually change.
If build errors persist it may be necessary to wipe the build directory completely (`meson setup --wipe`), it is recommended to use `ccache` to accelerate the subsequent rebuild.

If you want to use a custom version of a dependency library, install it first and then adjust one or more of the variables mentioned above or corresponding Meson options:

- If the dependency installs a `pkg-config` file (usually `lib/pkgconfig/*.pc`), append the directory containing these files to `PKG_CONFIG_PATH` or `-Dpkg_config_path`.
- In all other cases, append the install prefix (i.e. the directory with `include/` and `lib/` or `lib64/`) to `CMAKE_PREFIX_PATH` or `-Dcmake_prefix_path`.
- For select dependencies (i.e. if the dependency doesn't install `pkg-config` or CMake files and there is no matching `share/cmake-*/Modules/Find*.cmake` script in the CMake installation), you may also opt to configure the compiler instead, by:
  - Extending `-Dc_args` with appropriate `-I` include flags and `-Dc_link_args` with `-L` link flags, or
  - Extending `CPATH` with `include/` directories and `LIBRARY_PATH` with `lib/` or `lib64/` directories.

Some of these variables can be persisted as Meson built-in options (e.g. `-Dpkg_config_path`) or more generally [native files][meson native file].
For example, a native file to use GCC 11 with extra flags and a custom dependency search flags would look like:

```ini
# …/my-env.ini
[binaries]
c = ['ccache', 'gcc-11']
cpp = ['ccache', 'g++-11']

[built-in options]
c_args = ['-fstack-protector']
cpp_args = ['-fhardened']
cmake_prefix_path = ['/…/path/to/custom/install/', '/…/path/to/other/install/']
pkg_config_path = ['/…/path/to/custom/install/lib/pkgconfig', '/…/path/to/other/install/lib/pkgconfig']
```

And then can be used in a build directory by passing additional arguments to `meson setup`:

```console
$ meson setup --native-file …/my-elf-dyn.ini builddir/
```

See [`doc/developers/meson.ini`] for a more complete template listing the settings available in a native file.

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

[meson]: https://mesonbuild.com/
[meson native file]: https://mesonbuild.com/Native-environments.html
[meson subprojects]: https://mesonbuild.com/Subprojects.html
[meson wraps]: https://mesonbuild.com/Wrap-dependency-system-manual.html
[meson's `dependency()` docs]: https://mesonbuild.com/Reference-manual_functions.html#dependency
[spack]: https://spack.io/
[spack user's manual]: https://spack.readthedocs.io/
[`doc/developers/meson.ini`]: /doc/developers/meson.ini
