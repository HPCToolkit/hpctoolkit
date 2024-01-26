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

First install [Spack][getting spack] 0.20.1 (or higher) and configure for the current system.
Then create and install a Spack environment containing all the dependencies:

```console
$ spack env create my-hpctoolkit-deps doc/developers/example-deps.yaml
$ spack env activate my-hpctoolkit-deps
$ spack config edit  # Optional, customize for personal needs
$ spack install  # This will take a while
$ spack env activate my-hpctoolkit-deps  # Repeated to update environment vars
```

This environment (`my-hpctoolkit-deps`) needs to be activated for all `meson` commands while working with HPCToolkit.
See the comments in [`doc/developers/example-deps.yaml`] for additional details about how this environment must be configured.
While active, building HPCToolkit follows the standard Meson build sequence:

```console
$ meson setup \
  -D{c,cpp}_link_args="-Wl,-rpath=$(spack location -e)/.spack-env/view/lib -Wl,-rpath=$(spack location -e)/.spack-env/view/lib64" \
  builddir/
$ cd builddir/
$ meson compile  # -OR- ninja
$ meson test     # -OR- ninja test
```

As always, remember to clear the cache after altering the environment to capture any changed dependencies:

```console
$ spack config edit
$ spack concretize -f
$ spack install
$ meson configure --clearcache
```

For convenience, the HPCToolkit tools are easily available under the `meson devenv` shell:

```console
$ meson devenv
[builddir] $ hpcrun --version
```

### Configuration

Additional configuration arguments can be passed to the initial `meson setup` or later `meson configure` invocations:

- `-Dhpcprof_mpi=(disabled|auto|enabled)`: Build `hpcprof-mpi` in addition to `hpcprof`. Requires MPI.
- `-Dpython=(disabled|auto|enabled)`: Enable the (experimental) Python unwinder.
- `-Dpapi=(disabled|auto|enabled)`: Enable PAPI metrics. Requires PAPI.
- `-Dcuda=(disabled|auto|enabled)`: Enable CUDA metrics (`-e gpu=nvidia`). Requires CUDA.
- `-Dlevel0=(disabled|auto|enabled)`: Enable Level Zero metrics (`-e gpu=level0`). Requires Level Zero.
- `-Dgtpin=(disabled|auto|enabled)`: Also enable Level Zero instrumentation metrics (`-e gpu=level0,inst`). Requires Level Zero, IGC and GTPin.
- `-Dopencl=(disabled|auto|enabled)`: Enable OpenCL metrics (`-e gpu=opencl`).
- `-Drocm=(disabled|auto|enabled)`: Enable ROCm metrics (`-e gpu=amd`). Requires ROCm.
- `-Dvalgrind_annotations=(false|true)`: Inject annotations for debugging with Valgrind.

Note that many of the features above require additional optional dependencies when enabled.
See the example Spack environment ([`doc/developers/example-deps.yaml`]) for details on what dependencies need to be added.

### Advanced Meson: Dependencies

The Spack-based dependency management described in the Quickstart, along with the requirement to always keep the environment activated, can be limiting and confusing in practice.
This section is intended as a "peek behind the curtain" of how Meson understands dependencies, and an abridged version of [Meson's `dependency()` docs] for developers.

Meson fetches all dependencies from the "build environment" aka the "machine" (technically two, the `build_machine` and the `host_machine`, which are only different when [cross-compiling](https://mesonbuild.com/Cross-compilation.html)).
Whenever Meson configures (i.e. during `meson setup` or automatically during `meson compile/install/test/etc.`), queries the "machine" as defined by various environment variables:

- Finding programs (`find_program()`) is done by searching the `PATH`,
- The C/C++ compilers (`CC`/`CXX`), which are also affected by `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `INCLUDE_PATH`, `LIBRARY_PATH`, `CRAY_*`, ...
- Dependencies are found via `pkg-config`, which is affected by `PKG_CONFIG_PATH` (and other `PKG_CONFIG_*` variables),
- Dependencies are found via CMake, which is affected by `CMAKE_PREFIX_PATH` (and other `CMAKE_*` variables),
- Boost is found under `BOOST_ROOT`, and the CUDA Toolkit under `CUDA_PATH`

Many of these variables are altered by `module un/load`, `spack un/load` and other similar commands, including the `spack env activate` used in the quickstart.
Meson assumes the "machine" does not change between `meson` invocations, however if it does by running the above commands it is possible Meson's cache will not match reality.
In these cases reconfiguration or later compilation may fail due to the inconsistent state between dependencies.
This can be fixed by clearing Meson's cache (`meson configure --clearcache`), Meson will efficiently avoid rebuilding files if the compile command did not actually change.

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

### Advanced Meson: Wrapped Dependencies

If dependencies are required but not found in the build environment, Meson will download and build a default version of the missing dependency from source, by default.
This section is an abridged version of Meson's official documentation on [subprojects][meson subprojects] and [wraps][meson wraps].

Dependencies that are required but not found will fall back to suitable subproject, the subproject will be configured during `meson setup` and built from source during `meson compile`.
Dependencies may be always required, or may be required due to an enabled feature, e.g. `-Dopencl=enabled` but not `-Dopencl=auto`.
Any subprojects that are used as part of the build will be listed at the end of the `meson setup` output, for example:

```
  Subprojects
    opencl-headers    : YES
```

If this fallback behavior is not desired, it can be disabled with `meson (setup|configure) --wrap-mode=nofallback`.
With this, all dependencies must be provided by the build environment, except for forced-fallback dependencies configured with `meson (setup|configure) --force-fallback-for=dep1,dep2,...`.

Subprojects are stored as directories under `subprojects/`, and may be downloaded automatically by Meson from corresponding `subprojects/*.wrap` files.
These files describe the URLs and hashes to download the upstream sources from, as well as the dependencies provided by the subproject.
Automatic downloads can be disabled with `meson (setup|configure) --wrap-mode=nodownload`, in which case previously downloaded subprojects will be used as fallback dependencies but new ones will not be downloaded.
This configuration may result in not-found dependencies, with a message such as the following:

```
Automatic wrap-based subproject downloading is disabled
Subproject  opencl-headers is buildable: NO (disabling)
Dependency OpenCL-Headers from subproject opencl-headers found: NO (subproject failed to configure)
```

In this case, you can use `meson subprojects download opencl-headers` to explicitly download the listed subproject, e.g. from a machine with open internet access.
If no subproject is specified (`meson subprojects download`), all available subprojects will be downloaded.
Note that wraps may update when updating a local checkout, when this happens you may see a message such as:

```
WARNING: Subproject opencl-headers's revision may be out of date; its wrap file has changed since it was first configured
```

To fix this, run `meson subprojects update opencl-headers` to redownload and update the subproject, e.g. from a machine with open internet access.

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

[getting spack]: https://spack.readthedocs.io/en/latest/getting_started.html
[meson]: https://mesonbuild.com/
[meson native file]: https://mesonbuild.com/Native-environments.html
[meson subprojects]: https://mesonbuild.com/Subprojects.html
[meson wraps]: https://mesonbuild.com/Wrap-dependency-system-manual.html
[meson's `dependency()` docs]: https://mesonbuild.com/Reference-manual_functions.html#dependency
[spack]: https://spack.io/
[spack user's manual]: https://spack.readthedocs.io/
[`doc/developers/example-deps.yaml`]: /doc/developers/example-deps.yaml
[`doc/developers/meson.ini`]: /doc/developers/meson.ini
