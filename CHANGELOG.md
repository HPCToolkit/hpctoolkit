<!--
SPDX-FileCopyrightText: 2003-2024 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

# Changelog

All notable changes and releases to this project will be documented in this file.

Recent releases are formatted loosely based on [Common Changelog](https://common-changelog.org/), a more restrictive subset of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2024.01.1] - 2024-03-28

### Changed

- C compiler with atomics/threads support is now required ([!945](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/945)) (Jonathon Anderson)
- Diagnostic messages for `hpcstruct` and `hpcprof` etc. now go to stderr instead of stdout ([!994](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/994)) (John Mellor-Crummey)
- `hpcstruct`, `hpcproftt` and `dotgraph` are now executables instead of wrapper scripts ([!952](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/952)) (Jonathon Anderson)

### Added

- `hpcrun --version` now reports build configuration ([#752](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/752), [!999](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/999)) (Wileam Phan)
- `objcopy` is no longer a requirement to build ([#708](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/708)) (Jonathon Anderson)
- Add "instant" from-source builds using Meson and Spack ([!933](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/933), [#806](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/806)) (Jonathon Anderson)
- `hpcstruct` no longer requires `nvdisasm` to be available on the `PATH` ([#799](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/799)) (Jonathon Anderson)

### Removed

- Remove `hpclink` ([!984](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/984)) (Jonathon Anderson)
- Remove override in `hpcrun` for `libmemkind` and the `--default-memkind` option ([#810](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/810)) (Jonathon Anderson)
- Copies of dependency shared libraries are no longer installed ([#780](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/780)) (Jonathon Anderson)
- Remove generated Autotools files from the repo ([!947](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/947)) (Ben Woodard). `automake`, `autoconf` and `libtool` are now required to build from source.

### Fixed

- Support GCC 13.x ([!915](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/915)) (Jonathon Anderson)
- Support Dyninst 13.0 ([!942](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/942)) (Mark Krentel)
- Support Xed 2023.08.21 ([!980](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/980)) (Mark Krentel)
- Support Python 3.8 ([!958](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/958), [!962](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/962)) (Tao Zhou, Jonathon Anderson)
- Support Python 3.12 ([!944](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/944)) (Jonathon Anderson)
- Support CUDA 12.3 ([#826](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/826)) (Wileam Phan)
- Support ROCm 6.0 ([!1026](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/1026)) (Wileam Phan)
- Support GTPin 3.6 ([!936](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/936)) (John Mellor-Crummey)
- Support dependencies as installed by Fedora/RHEL packages ([!948](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/948)) (Ben Woodard)
- Fix support for incomplete implementations of C++17 ([!990](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/990)) (John Mellor-Crummey)
- Fix build error when assembly calls do not map to PLT by default ([!1018](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/1018)) (Wileam Phan)
- Fix consistency errors in `hpcstruct --gpucfg=yes` analysis ([#797](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/797)) (John Mellor-Crummey)
- Fix `hpcstruct` errors on very large GPU binaries ([!950](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/950)) (John Mellor-Crummey)
- Fix crash in `hpcprof` when `hpcstruct` GPU CFG analysis is incomplete ([!918](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/918)) (Jonathon Anderson)
- Silence inaccurate log warnings with `hpcrun -a python` ([!965](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/965)) (Jonathon Anderson)
- Avoid crash in `hpcrun` for applications with deep calling context ([!758](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/758)) (Jonathon Anderson)
- Avoid crash in `hpcrun` when OpenMP runtime has incomplete OMPT support ([!935](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/935)) (John Mellor-Crummey)
- Fix parent reporting for top-level OpenMP scopes, now under `<program root>` ([!992](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/992)) (John Mellor-Crummey)
- Work around crash in `hpcprof` when mutual recursion is present in GPU code ([#800](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/800)) (Jonathon Anderson)
- Warn and continue when `hpcprof --only-exe` cannot match anything ([#756](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/756), [!981](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/981)) (Jonathon Anderson)
- Fix `hpcrun` crash caused by unintentional self-monitoring ([!987](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/987)) (Wileam Phan)
- Fix `hpcstruct` crash on unreadable application binaries ([!1043](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/1043)) (John Mellor-Crummey)
- Fix `hpcprof-mpi` compatibility with MPI 3.1 implementations and others ([!1009](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/1009), [!1034](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/1034)) (Jonathon Anderson)
- Fixed a number of known compiler warnings in our code ([!973](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/973)) (Jonathon Anderson)

### Internal Changes

- CI pipeline runs now complete consistently under an hour.
  Exponential parts of our tested configuration matrix have been pruned.
  Pre-built software dependencies are now cached at multiple levels: individually using Spack's OCI buildcache feature, locally on the CI runner node using GitLab's `cache:` feature, and in batch as full ready-to-use container images.
  OCI-based caches are backed by GitLab's Container Registry and are publicly visible.
  The latter image-based cache is a unique feature, to contribute to the FOSS community we have published this as a CI/CD component ([`gitlab.com/blue42u/ci.predeps/build`](https://gitlab.com/blue42u/ci.predeps)) for other projects to reuse.
  (Jonathon Anderson)

- Symbol interception implementation has been separated from the corresponding logic.
  Internal symbols are no longer exposed to the application, no known bugs have been fixed by this but some crashes may be avoided.
  The primary purpose of this change is to pave the way for future enhancements to our library isolation and symbol interception mechanisms.
  ([!957](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/957)) (Jonathon Anderson)

- The regression test suite (`meson test`) is now available in all Git checkouts.
  The necessary test data is registered with Git instead of the Git LFS extension, improving the availability of the test suite in minimal software environments.
  ([#809](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/809)) (Jonathon anderson)

* CI failures now usually indicate real problems (Jonathon Anderson)
* The `./dev` build helper has been removed in favor of Meson-powered "instant" builds ([#807](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/807)) (Jonathon Anderson)
* CI scripts used to run `pre-commit` are now separated in a CI/CD component, [`gitlab.com/blue42u/ci.pre-commit/lite`](https://gitlab.com/blue42u/ci.pre-commit). ([!964](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/964)) (Jonathon Anderson)
* Indentation is now exclusively done using spaces when the language permits ([!911](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/911)) (Jonathon Anderson)
* All tools may now be run without installation ([#781](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/781), [!991](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/991)) (Jonathon Anderson)
* Add more tests for itimer and perf events and support testing in VMs ([!977](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/977), [!988](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/988)) (Jonathon Anderson)

## [2023.08.0] - 2023-08-12

```
HPCToolkit Release Notes

Enhancements
- Hpcrun
   - Intel GPUs
     - Improve support for Intel GPUs (hpctoolkit/hpctoolkit!873)
       - recognize Intel EM_INTELGT GPU ELF binaries
       - refactor the code to both Intel Patch Token binaries and Intel
         zeBinaries, even in the same execution.
       - fix race between gtpin instrumentation-based measurement and
         measurement via asynchronous sampling
       - improve GTPin support for instruction counting and latency
         measurements
       - adjust Level0 attribution to kernels to work with GTPin
     - Reduce level0 measurement overhead by having a measurement thread sleep
       rather than actively spinning
   - AMD GPUs
     - Improve support for ROCm hardware counters
       - only offer counters and derived metrics that make sense to
         aggregate throughout an execution. this means hiding metrics
         that compute percents per kernel launch.
       - add support for aggregate ROCm counters such as TCC_HIT_sum
       - manage all counters as double precision
     - Improve cross-version ROCm compatibility
       - support ROCm versions 5.1.x through 5.6.x
     - Ignore rocprofiler-created thread (hpctoolkit/hpctoolkit!868)
   - NVIDIA GPUs
     - Reverse call order when unsubscribing from CUPTI
   - General
     - Suppress reversed GPU intervals (hpctoolkit/hpctoolkit!848)
     - Resolution boost for CPU traces using -tt (hpctoolkit/hpctoolkit!837)
     - Fetch MPI rank from environment vars (hpctoolkit/hpctoolkit!850)
     - Abort samples that have too many stack frames to unwind
     - Attribute openmp target operation outside a target region
     - Fix support for monitoring Cray OpenMP (hpctoolkit/hpctoolkit!863)
     - Fix Clang 15 support
- Hpcstruct
   - Add new support for analyzing Intel ZeBinaries for GPUs. Integrate with
     support for analyzing Intel Patch Token binaries.
   - Add support for dyninst's unknown instruction API
     - Support parsing of Intel Sapphire Rapids binaries with AMX instructions,
       which are not understood by Dyninst using the unknown instruction API by
       using Intel's Xed to skip over these instructions to continue machine
       code parsing.
   - Use new hpcproflm utility to identify CPU and GPU binaries referenced by a
     measurements directory.
- Hpcprof
   - Unify logical Functions in MPI programs
     - Logical functions, identified by names, are used for Python and GPU
       kernels. Unify names across MPI ranks.
   - Repeat pread/pwrite on I/O truncation (hpctoolkit/hpctoolkit!858)
   - Fix source file path names in meta.db (hpctoolkit/hpctoolkit!853)
   - Keep loop contexts separate based on address (hpctoolkit/hpctoolkit!834)
     - The difference is that binary loops come from binary analysis, and so
       have a "point" (Module + offset) in addition to the
       "line" (File + lineno).  This enables loops on the same line
       (from loop fission from a Structfile) to remain distinct.
      - The "point" is added to the loop contexts in the meta.db using the
        optional "point" flex fields, thus requiring no change to the meta.db
        format.
    - Don't crash on error copying source files (hpctoolkit/hpctoolkit!841)
- Hpcproflm
     - A new helper utility for hpcstruct that computes load modules
       - Employ parallelism when scanning a measurement database to determine
         what CPU and GPU binaries are involved in the execution.
- Hpcproftt
     - Output name for RANK id tuple
- Hpcviewer
   - Free allocated resources to reduce memory footprint
   - Fix issue #68 (memory leaks) and issue #317 (incorrect trace scope)
   - Fix issue #311 (plot graph) and #312 (a database file is missing)
   - Improve hpcdata for better id tuple sorting by using
     integer and byte comparison instead of strings.
   - Update hpcdata to fix miscellaneous issues:
     #17, #309, #313, #314, #315, #317, #324, #329, #327, #328,
     #340
   - Fix issue #316: graph tooltip by not using system's tooltip
   - Fix issue #331, fix dark theme, remove unused classes.
     Closes #219, #134, and #331
   - Fix issue #325 (incorrect id tuple)
   - Fix issues #321, #309, and #312
   - Fix code smells
   - Fix issue #111 (wording consistency)
   - Update hpcviewer CI to latest practices
     - Pre-commit: Discourage working in main branch
     - All jobs now can run on the GitLab shared Runners.
     - `precommit` now uses a stock Python image.
     - The `test` jobs now use an X11 + WM image from a new dedicated
       repository: https://gitlab.com/hpctoolkit/x11-wm-docker
- Documentation
     - Fix doc and download page of hpcviewer
     - Update build instructions, packages.yaml.
- Continuous integration
     - Completely overhaul CI infrastructure.
     - Extend the ROCm testing matrix
     - Add new CI jobs to test the full breadth of ROCm versions. Now all ROCm
       versions 5.1-5.6 are tested both in `buildmany` and `check` capacities.
     - Outsource deps images construction
     - Don't use the Spack public buildcache
- Testing infrastructure improvements
     - Add test for hpcrun -L
     - Allow +1 CPU thread in ROCm tests
     - Correct Python for make-tar
- Meson builds
    - Add support for managing configuration, building, and testing with Meson
           We will eventually [convert the whole build system to Meson][1].
        Meson is as a rule much more restrictive than Autotools, so the details
        of our software is built will need to change. IMO for the better.
           Here, we provide a foundation for building with Meson by adding
        support for the `meson setup/compile/install/test` sequence. This gives
        a Meson skeleton to start with before converting the build system.
        ./dev meson ...` runs `meson ...` in devenvs for development purposes.
          The `tests2/` regression test suite is the first flesh on this
        skeleton, it is part of the Meson build and contains no Autotools. As
        such, a Mesonic build will always contain the tests2/ suite, and this
        test suite can only be run in a Mesonic build via `./dev meson test`.
        (This also provides a better CLI for testing than `make installcheck`
        ever could.) Note that, unlike a typical Meson build, this test suite
        needs to be run after a `./dev meson install`.
          For consistency, the dev.buildfe and CI now only perform builds using
        this Mesonic build sequence, instead of through Autotools. This will
        make build system refactors easier moving forward as well. The `+tests2`
        variant has been removed, it is now always on.
          Unlike Autotools, Meson provides feature-based options to select what
        features to enable, e.g. `-Dmetric_papi=enabled`. The dependencies in a
        devenv are provided by a generated Meson "native" file, and the Meson
        code handles the argument/CFLAGS composition to pass to Autotools.
        As such, `./dev env` is no longer needed and has been removed.
          Meson has a richer configuration space for optimization and debug
        flags. The Meson code will translate a number of these into their base
        CFLAGS and inject them into the Autotools build. The built-in options
        supported are `buildtype`, `debug`, `optimization`, `warning_level`,
        `werror`, `c_std`, `cpp_std`, `c_args` and `cpp_args`. See the Meson
        docs for what these all mean.
          The actual build happens within Autotools, there is very little change
        to that part of the build system. You can, for now, build "raw" with
       Autotools and get a working HPCToolkit, but Meson is recommended.
   - Fetch default compilers from Spack
```

## [2023.03.01] - 2023-02-28

```
HPCToolkit Release Notes

Enhancements:
- Hpcrun
  - Python Support
    - Add support for monitoring Python 3.10 applications as well as
      Python-wrapped applications, e.g. Kull.  Supports PyTorch as well.
  - AMD GPUs and ROCm
    - Support ROCm >= 5.3.0, which required monitoring kernels at the roctracer
      activity API rather than roctracer subscriber callback.
    - Support backwards compatibility across multiple ROCm versions.
      - HPCToolkit built against the newest ROCm 5.4.x can be used to profile
        applications compiled with older ROCm 5.y.z versions.
    - For ROCm, don't record GPU binaries to trace and profile at the kernel
      level. Instead, attribute to GPU kernel names. Demangle kernel names as
      necessary.
  - Intel GPUs and Level Zero.
    - A complete re-implementation of GTPin instrumentation for Intel GPU
      binaries using GTPin's C++ API. (The C API for GTPin is limited and
      deprecated.) Currently, only instruction counting is supported. Testing
      and validation was done using GTPin 3.3 (not publicly available yet).
    - Improve reporting of Level Zero error codes.
    - For Level Zero, don't record GPU binaries to trace and profile at the
      kernel level. Instead, attribute to GPU kernel names. Demangle kernel
      names as necessary.
  - NVIDIA GPUs and CUDA
    - Support CUDA >= 12.0.
  - OpenMP offloading support
    - Support for new OMPT capabilities first available in OpenMP 5.1, used by
      Intel's OpenMP offloading.
  - Add sanity checks for listing available GPU support
- Hpcstruct
  - Always enable "--gpucfg yes" when processing Intel GPU binaries
- Hpcprof
  - Release a new implementation of hpcprof-mpi that employs both
    distributed-memory parallelism (1 MPI rank per node) and extreme
    multithreading (using all but one of the hardware threads on a node)
    to analyze performance measurement data from extreme-scale executions.
  - Add "-V" option to show version information for consistency with other
    HPCToolkit tools
  - Add "-vv" option for more verbose diagnostic output
  - Improve warning message when processing binary DWARF/ELF information
    due to missing/incomplete Structfiles
  - Issue warning message when users run hpcprof on a measurement directory
    without running hpcstruct on it first
  - Improve warning message when encountering Structfiles that need hpcstruct
    to be run with "--gpucfg yes"
  - Without the "-vv" option, don't complain about callsite nodes in hpcstruct
    output that invoke target addresses that aren't declared as functions

Bug fixes:
- Hpcrun
  - To avoid runtime failures, don't set ROCm-related environment variables
    when HPCToolkit is not built with ROCm support
  - Fix compatibility with glibc >= 2.35 that previously caused hpcrun to
    collect zero measurement samples with "-e REALTIME" event
  - Suppress recording of GPU contexts with empty metric data
  - Fix bug on double freeing a rocprofiler URI
  - Support for using CUPTI on a subset of local MPI ranks to enable
    PC sampling measurements with GAMESS
  - Exit cleanly when CUPTI is initialized multiple times on the same GPU
  - Correctly attribute GPU metrics to the GPU kernel node to fix hpcviewer's
    bottom-up view
  - Copy missing GPU binary from device for NVIDIA GPU kernels generated
    from OpenMP target regions by Cray compilers
  - Fix bug where hpcrun could crash on Arm during synchronous unwinds
  - Avoid deadlock in initialization for AMD GPUs by initializing HPCToolkit
    upon entry to libc_start_main.
  - Avoid setting any environment variables in libhpcrun.so during ROCm
    initialization
  - Avoid recording AMD GPU binaries at present because Dyninst can't analyze
    them without failing
- Hpcstruct
  - Add "has-calls" attribute under <LM> tags in the Structfile format
    to record whether hpcstruct was run with "--gpu-cfg yes".
  - Increase default OpenMP stack size to 32 MB to prevent hpcstruct from
    running out of thread stack space
  - Until PC sampling capabilities on AMD GPU becomes available, avoid saving
    AMD GPU binaries at all, since analyzing these kernels currently crashes
    Dyninst
- Hpcprof
  - Avoid generating a trace.db database when no tracing data is available
  - Correctly output all previously-missing derived GPU metrics with their
    format settings
  - Add a "visible by default" flag to the metrics.yaml format.
  - Process incomplete/corrupt measurement files gracefully instead of crashing
    or dumping core.
  - Fix bug on processing derived GPU metrics from PC sampling metrics
  - Hpcprof command line output and hpcproftt now always prints metric ids
    in decimal format for consistency with profile.db and cct.db
  - Fix bug where parts of traces may occasionally appear empty in hpcviewer
  - Fix occasional crash when executing hpcprof with high thread counts
  - Fix bug that allowed hpcprof to copy source files outside the database
    directory

Infrastructure improvements:
- Build system:
  - HPCToolkit can now be built using LLVM/Clang >= 10
  - Fix long-standing, obscure bug that caused builds to fail on some
    filesystems that didn't handle timestamps correctly (the infamous
    "hpcrun-fmt.txt" bug)
  - Partial code migration from C to C++ to support GTPin >= 3.2.2
- CI improvements:
  - More integration tests for hpcrun, hpcstruct, hpcprof, and hpcprof-mpi
  - Validation tests for hpcprof's output database structure
  - Initial version of utilities for CCT validation
  - More CPU and GPU test codes
  - Better runner stability
  - Reproducibility of CI jobs and artifacts
  - Expand testing across more CUDA, HIP, and Level Zero+GTPin versions
  - Regenerate fresh test data as needed
- Automatic formatting and linting using pre-commit
- Code quality report in GitLab based on number of compiler warnings and errors
- Add issue and MR templates

Dependency changes:
- Additions
  - HPCToolkit now requires C++17 compilers to build; the minimum GCC version
    is now >= 8.x.
  - Python (>= 3.10): only used when building with experimental Python support.

Known Issues:
- LaTeX source files for the HPCToolkit User Manual have been split off from
  the main HPCToolkit repository. A PDF copy of the User Manual is still
  included.
- HPCToolkit now uses the roctracer activities API to record AMD GPU kernel
  names, introduced in ROCm 5.3.3. When profiling applications built with a
  ROCm stack < 5.3.3, HPCToolkit will not show GPU kernel names.
- HPCToolkit is blind to CPU samples in private C++ namespaces (including Intel
  Level Zero runtime), causes lots of samples that cannot be unwound at all

HPCViewer Release Notes

Enhancements:
- Hpcviewer now sorts traces by logical id instead of physical id (host, nodes, and cores)

Bug fixes:
- Fix bug when rendering table headers in GTK dark theme
- Fix bug in flat view's cost attribution for inlined statements
- Fix Java out of memory exception when filtering CCT nodes
- Fix bug for the thread selection for the thread view
- Fix error when displaying a plot graph in a filtered database
- Fix errors when using zoom-in/out and flatten operations
- Fix bug in the creation of derived metrics for meta.db databases
- Fix bug for metric's name and description
- Fix bug by removing helper accumulator metrics in the plot graph menu
- Fix bug in plotting exclusive costs in the plot graph
- Fix missing exclusive costs in the thread view
- Fix bug in x-axis plot graphs for meta.db databases
- Fix bug when flattening in the flat view for new meta.db databases
- Fix Java exception when sorting a metric column
- Fix bug incorrect file attribution in some call sites
- Fix bug when opening GPU databases with statistic metrics
- Fix bug for top-down cost attribution for the line scopes
- Fix bug when the first sample trace is not rendered properly
- Fix bug when exclusive costs disappears in the profile view
- Fix Java exception when filtering metric column and then adding a new
  derived metric
- Fix bug when hpcviewer doesn't render the last real trace entry correctly
```

## [2022.10.01] - 2022-10-06

```
HPCToolkit Release Notes

Enhancements:
- Hpcrun
  - Supports profiling and tracing GPU-accelerated applications atop Intel's
    Level0 runtime
  - Supports instrumentation-based performance measurement of GPU kernels atop
    Intel's Level0 runtime, including instruction counting and approximate
    attribution of memory latency
  - Reduces space requirements for performance measurements using new sparse
    formats
  - Accommodates the inability to fork when software for Cray's Cassini NIC
    senses the use of pinned memory
  - Annotates profiles and traces with metadata that identifies hardware and
    software abstractions (e.g., nodes, GPUs, MPI ranks, threads, GPU streams)
- Hpcstruct
  - Analyzes CPU and GPU binaries using half of the hardware threads available
    in your CPU set
  - Supports caching of program analysis results for reuse. For instance,
    analysis results of support libraries can be done once and cached rather
    than once for each experiment.
  - Automatically removes incomplete or otherwise invalid analysis results in
    your cache
- Hpcprof
  - Reduces space requirements for performance analysis metrics by replacing a
    dense representation of  metrics for CPU threads and GPU streams with
    sparse tensors
  - profile-major-sparse format - all metric data for an individual profile is
    contiguous (profile.db)
  - context-major-sparse format - all metric data for an individual calling
    context is contiguous (cct.db)
  - Assembles a single-file representation of program traces for all CPU and
    GPU contexts (trace.db) instead of having hpcviewer combine traces
  - Replaces XML representation of program calling contexts with a compact
    binary representation (meta.db)
  - Is now highly multithreaded: by default, it analyzes performance data using
    all available threads (in your CPU set)
  - Reconstructs calling contexts within GPU kernels, including dynamic call
    chains, inlined functions and templates, loop nests, and statements

Bug fixes:
- Eliminated a heap corruption error in hpcrun.

Infrastructure improvements:
- documentation for all of HPCToolkit's binary formats is automatically
  deposited in every database that hpcprof generates.
- development and deployment of CI for multiple architectures and operating
  systems
- development of ‘minitest' - a collection of CPU and GPU test cases
- GPU test cases currently exercise NVIDIA and AMD GPUs using CUDA, HIP, and
  OpenMP

Dependency changes:
- Deletions
  - binutils: replaced by elfutils
- Additions
  - libiberty: was part of Binutils but is now required separately
  - yaml-cpp: a YAML parser and emitter in C++

Known Issues:
- HPCToolkit's hpcprof-mpi is unavailable in this release while we improve its
  robustness for corner cases so that it is suitable for production use. When
  finished, it will employ both shared and distributed memory parallelism for
  scalable analysis of performance measurements.
- On AMD GPUs, measurements of activity on AMD GPUs using hardware counters
  don't include measurements of OpenMP offloading implemented directly atop
  HSA by AMD's compilers.

HPCViewer Release Notes

Enhancements:
- Supports new binary representations (profile.db, cct.db, trace.db, meta.db)
- Supports using CCT node Filters to hide nodes and/or their descendants when
  rendering traces
- Reports tuples of metadata associated with trace lines
- Supports filtering of trace view using trace metadata

Bug fixes:
- Fixed rendering of traces for GPU streams so that they appear properly at all
  scales
- Cleared memory used in a pointer-based data structure when filtering tree
  nodes to enable garbage collection to avoid a JVM out of memory error
- Fixed resizing of table rows in filter dialog box
```

## [2022.05.15] - 2022-05-16

```
Bugfixes since 2022.04.15

- hide the symbols from XED, this avoid a problem interfering with
  symbols from Intel gtpin
- fix a typo in renamestruct.sh script
- fix a problem with fork() on Cray
```

## [2022.04.15] - 2022-04-22

```
HPCToolkit Release Notes

Enhancements:
- Hpcrun includes initial support for using the OpenMP OMPT interface for
  profiling and tracing of OpenMP TARGET operations on AMD GPUs in code
  generated by ROCM 5.1's clang-based AOMP compiler.
- Hpcrun supports profiling of kernels on AMD GPUs using publicly available
  hardware counters with AMD's rocprofiler API.
- Hpcrun obtains binaries for code that executes on AMD GPUs using AMD's
  Roctracer API instead of ROCm Debug API.
- Hpcrun emits a better error message when an application unexpectedly closes
  hpcrun's log file.
- Hpcrun now uses an embedded implementation of an MD5 hash function for
  naming CPU and GPU binaries revealed in memory.
- Hpcstruct now supports caching of structure files from binaries it analyzes.
  A cache greatly reduces the time to analyze binaries for executions as the
  cache will almost always contain up to date analysis results for commonly
  used shared platform libraries, e.g. libc, libm, as well as libraries for MPI.
  When a binary changes, results in the cache are updated as needed.
- Hpcstruct no longer pretty-prints its output by default. Omitting leading
  blanks due to pretty printing reduced the output size by over 15%, which was
  quite significant when analyzing multi-gigabyte binaries.
- When applied to a measurements directory, hpcstruct will analyze only CPU and
  GPU binaries that were measured in the execution using a mix of parallelism
  and concurrency.  Binaries that did not get any profile hits are not analyzed.
- Hpcstruct's parallel efficiency has been improved. Changes that contributed
  to that improvement include enhancements to parallelism in Dyninst's
  finalization of binary analysis and parallel assembly of hpcstruct's output
  file.
- Update hpcstruct to support analysis of CUDA binaries from 11.5+ to
  accommodate change to NVIDIA's nvdisasm output format.
- When measuring hardware counter metrics for kernels on AMD GPUs, disable
  kernel measurement with Roctracer because it gives an incorrect timestamp
  for the first kernel. The timestamp is wrong by a mile and destroys the
  accuracy of kernel profiles and traces.

Bug fixes:
- Adjust tracing for ROCm GPU activities to correct alignment between CPU and
  GPU timelines.
- Fix use of Dyninst by hpcstruct so that it sees inlining info in Intel GPU
  binaries.

Infrastructure improvements:
- Code for hpcrun's use of LD_AUDIT has been streamlined.
- Fixed recording of program path names as part of metadata in hpcrun's
  output files.

Dependency changes:
- Deletions
  - Mbedtls - superseded by internal MD5 hash implementation
  - ROCm Debug API - obtain GPU binaries using Roctracer API instead
  - Gotcha - unused and removed
- Additions
  - Rocprofiler API - included for a spack '+rocm' install to provide access
    to hardware counters on AMD GPUs
  - HSA - included for a spack '+rocm' install to support rocprofiler

Known Issues:
- Profile measurements and traces for AMD GPUs, which are new for ROCm 5.1,
  should be viewed with some skepticism.
    Also, elapsed time for copies seem too large for executions that we've
  measured. For a 96-thread run of miniqmc, the aggregate time for copies
  reported by AMD's OMPT implementation for its GPUs was almost 100x longer
  than the real time of the execution. If timestamps are incorrect for
  OpenMP events on AMD GPUs, this will affect the accuracy of both profile
  and trace views.
    Furthermore, trace items for OpenMP events on AMD GPUs are known to
  overlap. For that reason, having hpcviewer render them on a single
  trace line, which it does, is problematic. As a result, overlapping
  trace items will cause incorrect statistics in trace view. In such
  cases, the profile view will accurately represent the aggregate values
  reported by OMPT for AMD GPUs.
- In some cases, attribution of exclusive metrics for BLOCKTIME and
  CTXT SWTCH to call paths within the Linux kernel may be missing
  even though inclusive costs for these metrics are attributed properly.


HPCViewer Release Notes

Enhancements:
- Improved call site icons
- Double buffering x and y axis in the trace view
- Simplify metric number in derived metrics
- Set maximum database history to 20
- Set the default GPU trace exposure to true

Bug fixes:
- Fix issue #155 (row selection is not properly refreshed)
- Fix issue #157 (font size on the table)
- Fix issue #161 (horizontal scroll bar on the source pane)
- Fix issue #162 (context menu "Find")
- Fix issue #168 (invalid metric index when merging 2 databases)
- Fix issue #169 (incorrect column size when switching from top-down to
  bottom-up views)
- Fix issue #177 (truncated row selection)
- Fix issue #179 (three dots in the statistic view)
- Fix issue #188 (incorrect column size in bottom-up and flat views). Related
  to issue #169
- Fix issue #189 (selected row is not refreshed after expanding a node).
- Fix "Apply to all views" option in the metric view for a merged database
- Fix an error in the preference window
- Fix row resizing in the statistic view
```

## [2022.01.15] - 2022-01-18

```
Bug fixes
  - fix thread memory pool for amd gpus
  - fix hpcrun use of ignore thread
  - cleanup gpu traces
  - fix auditor initialization bug

hpcrun
  - add spack smoke test

hpcviewer
  - Using NatTable widget instead of SWT table for better scalable data representation.
  - Support for more than two databases in a window (issue #135)
  - Search a text in the tree table (issue #142)

  Issue #20 (partial fix): enhancement for x-axis in the trace view
  Issue #62 (partial fix): render tiny gpu trace when it's possible
  Issue #101: fix clipped figures in the statistics view
  Issue #112: fix white dots on the trace view (Windows only)
  Issue #113: fix trace view refresh problem (Windows only)
  Issue #118: fix computing the ideal width of table columns
  Issue #121: fix tree image in the bottom-up view
  Issue #126: fix illegal argument exception when flattening a tree
  Issue #127: fix dark mode on macOS
  Issue #128: fix -v option on Linux command line
  Issue #132: fix font color for row selection on macOS
  Issue #134: fix dark mode in Linux GNOME window manager
  Issue #137: fix a spacer in a leaf node
  Issue #138: fix call site icon position in the table
  Issue #139: fix data concurrency problems
  Issue #140: fix row number in the metric view
  Issue #141: ensure consistency between metric view and thread view
  Issue #143: fix support for ISO 8859 character set
  Issue #145: fix the column width when resizing the window or the table
```

## [2021.05.15] - 2021-06-09

```
Bug fixes

  hpcrun
    CPU issues
      - avoid deadlock by not sampling an openmp thread before it finishes
        setting up TLS
      - avoid having the UCX communication library used by MPI terminate
        a program when an unwind fails rather than just dropping a
        sample
      - fix initialization of control knobs when a process forks but
        does not exec
      - add a timeout to interrupt a hung cuptiActivityFlushAll and so a program
        can terminate and write out all performance data already collected.
    Intel GPUs
      - always dump Intel GPU binaries so we can extract kernel names
        even if not using GTPin binary instrumentation
    NVIDIA GPUs
      - avoid introducing kernel serialization while using coarse-grain
        measurement by monitoring CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL rather
        than CUPTI_ACTIVITY_KIND_KERNEL

  hpcstruct
    - correct reconstruction of loop nests for Intel GPU binaries

  hpcviewer
    - Fix issue #80 and #81 (null pointer exception for empty databases)
    - Fix issue #79 (CCT filter on the trace view, preserve tree expansion)
    - Fix issue #73 (sort direction is not shown on Linux for the first appearance)
    - Fix issue #75 (closing only a window in multiple windows mode)
    - Fix issue #74 (no sort direction on Linux/GTK)
    - Fix issue #85 (keyboard shortcut to minimize the window)
    - Fix filtering CCT nodes for thread views
    - Fix hot path to select the child node instead of the parent
    - Fix merging GPU databases which contain aggregate and derived metrics
      by deep copying the metric descriptors.
    - Fix build script to include notarization for mac
    - Fix storing recent open database: store the absolute path, not the relative one.
    - Fix SWT resource leaks
    - Fix flickering issue on Windows when splitting the hpcviewer window.
    - Fix trace view's color map changes to also refresh other panes and windows
    - Fix Find dialog layout on Linux/GTK
    - Fix merging GPU databases
    - Fix a procedure-color mapping bug in the trace view
    - Partial fix issue 42: Fix a performance bug when sorting a table

Improvements

  hpcviewer
    - Improve the performance of hot-path operation by not re-revealing the tree path.
    - Default window size is 1400x1000 or the screen size
    - Trace view: Move depth field into a separate pane so users can change the depth easily
      even when call stack view is not visible.
    - Reduce memory consumption.
    - Use Java XML parser to slightly improve XML parsing performance and avoid using
      the old Apache xerces.
    - Code clean-up, remove dead code and remove unused variables
    - Issue 77: Add support for different color mapping policy in the trace view.
      Default: procedure-name color instead of random color.
    - Warn users when filtering is enabled
    - Default is to build with Eclipse 4.19 (2021.03) except for Linux
      ppc64le (built with Eclipse 4.16). Some fixes include improved dark color theme.

  dotgraph
    - enhance dotgraph to dump control flow graphs for GPU binaries
```

## [2021.03.01] - 2021-03-01

```
The principal objective of this release was to support profiling for
various GPU-based applications, under a variety of programming methods,
and for a variety of GPUs.

New and Improved functionality
  hpcrun
    Support for GPU Profiling and Tracing
    - Intel GPUs
      - Coarse-grain profiling of GPU operations
        (kernel launch, memory copy, synchronization, etc.)
        - for OpenCL programs
        - For dpcpp programs using either OpenCL or Level 0 runtimes

            Tracing of GPU operations for OpenCL programs or dpcpp programs
            when using the OpenCL runtime

            Fine-grain measurements (instruction counts) using GT-Pin
            on OpenCL or dpcpp programs using the OpenCL runtime

          Support for instruction counts with GT-Pin
            For OpenCL or dpcpp  programs using the OpenCL runtime,
            including attribution of performance to source lines

    - AMD GPUs
      - Profiling and tracing of GPU operations for HIP programs
      - Support for OpenCL programs
    - Nvidia GPUs
      - Profiling and tracing of OpenCL programs
      - Fine-grain measurement of GPU kernels with PC sampling
   Support for CPU profiling
   - Improved Handling of Dynamic Libraries
     - Use LD_AUDIT to track loading and unloading of shared libraries;
       enabled by default; may be disabled if necessary.
   - Data Collection for Flat-profiles
     - Adds resilience by bypassing stack unwind to avoid problems with new OSs,
       libraries, compilers, etc.
   - Improved Handling of Exported and Imported Interfaces
     - Avoids conflicts with applications and other tools.
  hpcstruct
    Support for Intel and AMD GPU binaries
    - Improved Processing of All GPU binaries
      - Better handling of line-numbers, inlining information, loops, and
        statements.
    - Improved Handling of Paths
    - Improved Performance by Analyzing Binaries in Parallel

  hpcprof
    Adjustments to Hide Detailed GPU Metrics by Default

  hpcviewer and hpctraceviewer
    Merge the Two Views into a Single hpcviewer Command
    - The integrated interface enables one to view both profiles and traces.
      The new user interface, based on Eclipse 4, works with Java versions 8-14.
      The integrated interface can be built from source on MacOS, Windows, and
      Linux using Apache Maven.
    Enhance Filter Panel
    - Make Panel resizable, and add an "Apply" button.  Trace filters can use a
      regular expression for filtering ranks.
    Improved Preferences, About, Info, and Log Dialog Boxes
    - New support for log files using slf4j.  Logs are stored in the same
      workspace and can be cleared.
    Improved Color Management
    - Colors are now allocated lazily.
    Improved Performance
    - Especially with respect to expanding or contracting trees, and for handling
      very many metrics.  Reduced memory usage for the viewers.
    Beta Support for ARM Machines
    - Eclipse has beta support for ARM.
    Additional Release Notes for hpcviewer
     For more information, see https://github.com/HPCToolkit/hpcviewer.e4/releases

Fixed Issues

- Overestimation of CPU time for a Thread that is Sleeping
  - Previously CPU time for a sleeping thread would be incremented by the
    real-time the thread was sleeping; it is now correctly incremented only by
    CPU-time.  The issue is not yet fixed in Trace View.
- Mishandling of map/unmap/map of Dynamic Libraries
  - When a dynamic library is unmapped and a second dynamic library is mapped to
    the same region of the address space, samples may not be properly attributed.
- Conflicts with Application Libraries
  - HPCToolkit uses libunwind to interpret compiler-recorded unwinding
    information and libxz for compression. An application using a different
    version of these libraries could fail if its calls are bound to HPCToolkit's
    copies. The entry-points in HPCToolkit's copies of libunwind and libxz are
    now invisible to applications.
- Incorrect Interactions with UCX
  - UCX is a communication substrate used by OpenMPI. UCX wraps mmap and munmap
    and calls from an HPCToolkit signal-handler to mmap or munmap could be
    intercepted by UCX and cause a program to hang or fail. HPCToolkit now uses
    private versions of mmap and munmap that cannot be intercepted by other
    layers in the software stack.
- libmonitor Problems Relating to fork and Thread-Termination
  - Patches were issued to fix crashes or deadlocks relating to these issues.
- Problems with Applications Using RUNPATH
  - The RUNPATH settings are now properly handled to find dynamic libraries
- Workaround for LD_AUDIT Bug Present before glibc 2.32

  - LD_AUDIT transparently converts dlmopen calls to dlopen to merge all shared
    libraries into the global namespace. If that
    doesn't work, you ask hpcrun to use an alternative to LD_AUDIT
    using --disable-auditor. This alternative provides full support
    for dlmopen with multiple namespaces. However, any executables or
    libraries that depend upon a RUNPATH attribute will only work when
    HPCToolkit uses LD_AUDIT to track dynamic library operations. When using
    --disable-auditor, one can use --namespace-single or --namespace-multiple
    to explicitly control whether dlmopen calls should be converted to
    dlopen or not, respectively.

- Workaround for LD_AUDIT Performance Degradation
  - Applications with many interlibrary calls suffer a performance degradation
    when LD_AUDIT is used. To address this issue, hpcrun rewrites an
    application's Global Offset Table (GOT). As a new feature, there is
    a potential that this may cause something to go wrong and cause your
    application to crash. If that happens, the rewriting can be disabled
    with the hpcrun argument --disable-auditor-got-rewriting to diagnose
    whether that was why hpcrun caused your application to crash.

Open Issues
- CPU Time is Shown Continuously in Trace View
  - CPU time is divided between the last event before the thread sleeps and
    the next event after the sleep; it should be blank when thread is sleeping
- Problems with PC Samples from NVIDIA GPUs and CUDA 11.2
  - HPCToolkit does not properly attribute PC samples collected for binaries
    generated by CUDA 11.2. NVIDIA changed the format of line map information
    produced by nvcc in CUDA 11.2. HPCToolkit uses Red Hat's Elfutils to parse
    line map information in CPU and GPU binaries. Elfutils does not yet parse
    NVIDIA's extensions to the line map, which leads to incorrect attribution.
- Problems Installing for  AMD GPUs
  - Installation of ROCm either as an installed version or as spack-build
    package requires a two-step build, specifying ROCm as external packages.
- Problems Installing for  Intel GPUs
  - Installation for Intel GPUs requires manual configuration and build of
    HPCToolkit. Spack can be used to build all HPCToolkit dependencies other
    than Intel components. Intel's software (oneapi, GT-Pin, Metrics Discovery
    API, and Intel Graphics compiler) must be built manually and have their
    paths provided to HPCToolkit's configure as they are not available as spack
    packages.
- Problems with Instruction-level Analysis of Intel GPU Binaries
  - Instruction-level analysis only works with Intel's newest compute runtime,
    which has not been publicly released.
- Moving Measurements Directories may Cause Problems
  - Recorded data contains absolute paths, and processing the data may not find
    the files needed to generate the database.
- Issues Concerning with hpcviewer
  - hpcviewer requires Java 8 or newer; it may fail on MacOS Big Sur because of
    Eclipse bugs.

Improved Installation, Configuration, and Build
- Restructure Build to Hide Symbols from Third-party Libraries
  - Use objcopy, and +pic options for those libraries to avoid conflicts with
    applications using the same libraries.
  - Rewrite Configure Script to Remove Manual Dependencies
- Update Dependencies to Latest Versions and Options
  - Specify spack packages for intel-xed, libunwind, libmonitor, comgr,
    llvm-amdgpu, libpfm4 (perfmon), and xz (lzma).  Use binutils+nls to avoid
    spack conflicts with other packages.
- Update Build of hpcviewer to use Maven
  - Build can now be done from the command line.
```

## [2019.08] - 2019-08-14

```
Enhancements

Build

Replaced hpctoolkit-externals with building prerequisites from spack.
See: http://hpctoolkit.org/software-instructions.html
or README.Install

Moved to using Dyninst 10 to provide better binary analysis used by
hpcstruct.

Hpcrun

Hpcrun can now tag some metrics to be hidden by default in
hpcviewer. This support was added to enable the full accounting for
the myriad GPU instructions stall reasons to be hidden by default.

Launch scripts are more robust for finding the install directory, and
provide an option for identifying the git version (branch and commit).

Hpcstruct

Hpcstruct now exploits threaded parallelism to analyze large
application binaries quickly.  Use 'hpcstruct -j num binary' to
specify number of threads.

Improved handling of irreducible loops.

Hpcviewer

Hide metric columns if it doesn't have metric values.

Using raw metric values in copying to clipboard and exporting to CSV
files, instead of exporting the displayed format of the value.

Bug fixes

hpcprof:

Fixed bug so that inlined procedures are now merged in bottom-up and
flat views. Previously, an internal buffer overflow prevented fusion
of some instances of inlined procedures in bottom-up and flat views.

Hpcrun

Fixed handling of dynamic thread allocation.

Fixed handling of precise ip modifier. Using Linux perf convention to
force hpcrun to enable precise-ip attribute (:p for slightly precise,
:pp for mostly precise, :ppp for exactly precise).

Fixed handling of memory barrier when manipulating buffer of events
recorded by Linux perf. Incorrect memory barrier handling caused some
event records to be handled incorrectly.

Fixed kernel blocking sample-source. Context-switch attribute is now
supported on Linux kernel 4.3 or newer. It is also available on RedHat
3.x or newer, which has a backport of the kernel support.

Hpcviewer

Fixed filtering nodes on Flat view.

----------

Merged the 'banal' branch into master.  This fully replaces hpcstruct
and the banal code with the new inline tree format.  However, the
format of the .hpcstruct file is unchanged.

The new banal code produces a better loop analysis (placement of loop
header) and has a smaller memory footprint.  This also includes
support for displaying parseapi gaps (unclaimed code regions) in the
viewer.

Merged the 'perf-datacentric' branch into master.  This adds support
for the perf family of events as a way of accessing the hardware
performance counters without using PAPI.
```

## [2017.06] - 2017-06-29

```
Updated the 'ompt' branch and merged master into ompt.  This improves
scalability of the ompt support to large thread counts, as found on
KNL and Power8. The ompt branch, together with the LLVM OpenMP runtime
library provides

- support for attributing performance measurements to a global-view,
  source-level program representation by unifying measurements of OpenMP
  activity in worker threads with the main thread.
- support for attributing causes of idleness in OpenMP programs using
  HPCToolkit event OMP_IDLE.

The LLVM OpenMP runtime can be used with Clang, Intel, and GNU compilers.
The LLVM OpenMP runtime with OMPT support for performance analysis
can be can be obtained as follows

  svn co http://llvm.org/svn/llvm-project/openmp/trunk openmp

To use the draft OMPT support present in LLVM's OpenMP runtime, one must
build the LLVM OpenMP runtime with LIBOMP_OMPT_SUPPORT=TRUE. This version
of the runtime is designed to be used with HPCToolkit's ompt branch.

  Note: support for the OMPT tools interface in HPCToolkit and LLVM
  currently follows a design outlined in TR2 at openmp.org. An improved
  version of the OMPT interface was designed for OpenMP 5.0, which
  is described in TR4 at openmp.org. At some point soon, both LLVM and
  HPCToolkit will transition to support the new API; presently, both lack
  support for the new API.

Merged the 'atomics' branch into master.  This branch replaces the
atomic operations in hpcrun with C11 atomics through the stdatomic.h
header file.

Updated Dyninst to version 9.3.2 in externals, plus a patch for better
binary analysis of functions that use jump tables.

Updated hpcstruct to handle new ABI on Power/LE architectures, which
has both internal and external interfaces for functions.


Various bug fixes:

-- enhanced binary analysis for call stack unwinding on x86-64, including
an enhancement to analyze functions that perform stack alignment and
a bug fix to track stack frame allocation/deallocation of space for local
variables using the load effective address (LEA) instruction.

-- fixed bug in hpcrun to correct data reinitialization after fork(). This
bug prevented using hpcrun to profile programs launched with shell scripts.

-- fixed bug in hpcrun in binary analysis related to ld.so.

-- fixed bug in hpcstruct in getRealPath() that caused hpcstruct
to sometimes report incorrect file names.


Known issues:

When profiling optimized code with HPCToolkit, one may find that a program
generates a significant number of "partial unwinds" where the call stack
can't be unwound all the way up to main.  This more commonly happens on
x86-64 architectures than on PowerPC and ARM. A large number partial unwinds
may make it harder to use the top-down calling context view in hpcviewer,
which works best when call stacks unwind all the way up to main. Even
with significant numbers of partial unwinds, the bottom-up caller's view
and the flat view in hpcviewer can be used effectively for analyzing
performance. Ongoing work aims to improve call stack unwinding of
optimized code by employing compiler-generated unwinding information
where available in addition to using binary analysis to discover unwinding
recipes.

On x86-64, hpcfnbounds occasionally is too aggressive about inferring the
presence of stripped functions in optimized programs. We have noticed
this particularly for optimized Fortran. This can cause "partial unwinds",
where a call stack can't be unwound fully up to main.  Improving this
analysis is the subject of ongoing work.

When using with the LLVM OpenMP runtime's OMPT support, measurements
of programs compiled with GCC using HPCToolkit's ompt branch sometimes
reveal implementation-level stack frames that belong to the OpenMP
runtime system.  This will improve with the transition of HPCToolkit
and the LLVM OpenMP runtime to the new OMPT ABI designed for OpenMP
5.0. This transition should occur over the next 6 months. In the
meantime, there is nothing wrong with the quality of the information
collected. The only problem is HPCToolkit's measurements reveal more of
an implementation-level view of OpenMP than intended.
```

## [2016.12] - 2016-12-28

```
Added preliminary measurement, analysis, attribution, and GUI support
for Power8/LE.

Added preliminary measurement, analysis, and attribution support for
ARM64.

Added support for KNL, Knights Landing (configure as x86-64).

Overhauled data structures for managing shared state (binary analysis results)
in hpcrun to avoid mutual exclusion in the common case. This improves manycore
scalability.

Overhaul binary analysis to better attribute performance to highly-optimized
code that involves inlined functions, inlined templates, outlined OpenMP
functions.

Removed dependence on a locally-modified copy of binutils and switched
to use binutils 2.27, which supports Power8/LE and ARM64.

Updated build infrastructure to use newer versions of autotools that that
recognize the Power8 little-endian system type.
 - autoconf 2.69.
 - automake 1.15.
 - libtool 2.4.6.

Use boost version 1.59.0.
Use dyninst version 9.3.0.
Use elfutils version 0.167.
Use libdwarf version 2016-11-24.
Use libmonitor version from Sept 15, 2016.
Use libunwind version from Feb 29, 2016.

Known problems:

HPCToolkit's GUI's are not yet available for ARM64 platforms.

Binary analysis on Power8/LE may fail to fully analyze routines that contain
switch tables. This has several effects.
(1) Samples attributed to code regions in a routine that are overlooked by
    the binary analyzer will be attributed to the first source line of the
    enclosing routine.
(2) Loops in code regions that are overlooked will not be reported in
    hpcviewer.
```

## 5.4.x - 2015-12

```
Merged the hpctoolkit-parseapi branch into trunk.  Overhaul hpcstruct
and banal code to use ParseAPI to parse functions, blocks and loops.
Rework how hpcstruct identifies loop headers to use a top-down search
of the inline tree.

Partial merge of the non-openmp features from the hpctoolkit-ompt
branch into trunk.  Support for name mappings for <program root>,
<thread root>, etc, removing redundant procedure names, and better
name demangling.

Support for filtering nodes in the viewer, allows hiding a subset of
the CCT nodes according to some pattern.

Move repository to github.
```

## 5.3.2 - 2012-09-21

```
Fix a few show-stopper problems related to recent upgrades on jaguarpf
(interlagos) at ORNL since 5.3.1.

- hpcrun
  - add support for decoding the AMD XOP instructions.  this was
    causing many unwind failures on jaguarpf.
  - fixed a bug in hpcrun-flat that was breaking the build with
    PAPI 5.0

- externals
  - fixed symtabAPI to handle the new STT_GNU_IFUNC symbols in the
    latest glibc.  this was breaking hpclink on jaguarpf.
```

## 5.3.1 - 2012-08-27

```
- hpcrun
  - add documentation about environment variable HPCTOOLKIT, needed for
    use on Cray XE6/XK6
  - add support to use HPCRUN_TMPDIR or TMPDIR. On Cray platforms,
    /tmp is unavailable
  - don't try to integrate calling context trees for threads into the
    calling context tree for the main thread.
  - miscellaneous changes to satisfy stricter dependence checking by gcc 4.7
    and include files that omit unistd.h on Debian Linux
  - ensure that partial unwinds are properly categorized
  - address portability issues with idleness sample source, which supports
    blame shifting for OpenMP

- hpcprof
  - handle case when measurements directory and database directory are on
    different file systems. in this case, files can't be simply unlinked and
    relinked.

- hpctraceviewer
  - add support for filtering out threads and processes that are not of
    interest.
  - add -j option to install script to enable installer to curry a path
    for the java required into the hpctraceviewer script
  - fix memory leak for SWT native objects

- hpcviewer
  - support a mix of flattening and zooming in flat view
  - add new formatting options for derived metrics
  - add -j option to install script to enable installer to curry a path
    for the java required into the hpctraceviewer script

- externals
  - fixed problem with GNU binutils on 64-bit PowerPC that caused performance
    data to be misattributed because function "D" symbols were overlooked
  - added support to disable thread tracking (needed for CUDA support)

- update documentation
```

## 5.3.0 - 2012-06-25

```
- hpcrun
  - add an IO sample source to count the number of bytes read and written
  - add a Global Arrays sample source
  - add an API for the application to start and stop sampling
  - get the process rank for gasnet and dmapp programs (static only)
  - have hpcrun compress recursive calls by default
  - early support for a plugin feature
  - strengthen the async blocks to better handle the case of running
    sync and async sample sources in the same run
  - add a check that hpctoolkit and externals use the same C/C++ compilers.
    this should all but eliminate the 'GLIBCXX not found' errors

- externals
  - binutils: put the line info table into a splay tree.  for some
    BlueGene binaries, this speeds up hpcstruct by as much as 20x
  - libmonitor: update to rev 140.  this better handles signals and
    the side threads from the Cray UPC compiler
  - open analysis: bug fix to avoid missing some loops
  - symtabAPI: fix an off-by-one error that sometimes caused a
    segfault (thanks to Gary Mohr at Bull)
  - xerces: fix a compilation bug on BlueGene
```

## 5.2.1 - 2011-12-30

```
A performance enhancement and maintenance release.

- hpcrun
  - instead of discarding partial unwinds, record them in a special subtree
  - create option for compressing call paths with (simple) recursive
    function invocations
  - rework representation of call path profile metrics to support
    collection of tens or hundreds of metrics per run
  - program PAPI events to count during system calls
  - correct several subtle monitoring problems
  - improve performance of memory leak detection
  - fix how hpcrun opens files to be robust on systems where gethostid
    or getpid don't behave as expected

- hpcviewer includes key performance enhancements:
  - rapidly render scatter plots of metric values over all "MPI ranks"
    (or threads)
  - several bug fixes

- hpctraceviewer
  - significantly enhance rendering performance
  - several bug fixes
```

## 5.1.0 - 2011-05-27

```
- Full support for analyzing 64-bit Linux-POWER7 binaries [hpcprof, hpcstruct]

- To profile/trace large-scale executions, add support for sampling
  processes to measure.  Experimental. [hpcrun]

- W.r.t. binary analysis to compute unwind information for call path
  profiling: improvements computing function bounds on partially
  stripped code (32-bit x86 and libc on Ubuntu) [hpcfnbounds]

- Monitoring threaded applications: fix a rare race condition tracking
  thread creation/destruction [libmonitor]

- When analyzing multiple measurement databases: resolve certain bugs
  [hpcprof/mpi]

- When analyzing measurement databases in parallel: resolve a bug that
  could cause summary metrics for certain Calling Context Tree nodes
  to be compute incorrectly. [hpcprof-mpi]

- Upgrade libraries for reading binaries [hpctoolkit-externals]
  - upgrade to Symtab API 7.0.1
  - upgrade to libelf 0.8.13
  - upgrade to libdwarf-20110113
```

## 5.0.0 - 2011-02-15

```
- HPCToolkit supports Cray XT, IBM BG/P, generic Linux-x86_64 and Linux-86

- Major overhaul of hpcrun:
  - fully support dynamic loading
  - fully support gathering thread creation contexts
  - re-enable support on x86 for tracking return counts
  - use a CCT where a node keeps its children in a splay tree

- Rework post-mortem analysis tool, hpcprof:
  - create scalable tool (hpcprof-mpi)
  - unify hpcprof and hpcprof-mpi interface (and, when possible, internals)
  - automatically compute summary metrics in both hpcprof/mpi
    (using both incremental and 'standard' methods)
  - rework how hpcprof/mpi searches for source files to 1) find files
    more frequently; and 2) perform much more rapidly when multiple
    source files must be found (the common case).  No longer enter an
    infinite loop if symbolic links create a cycle.
  - support for --replace-path

- Rework hpcviewer:
  - incrementally construct Callers View
  - correctly construct Callers and Flat Views, even with summary
    metrics (and without thread-level metrics)
  - generate scatter plots of out-of-core thread-level metrics
  - correctly expand hot path

- Rewrite hpcsummary

- Create (and recreate) a Users Manual

- Updates to HPCToolkit's externals
  - full support for configuring/building from an arbitrary directory
    and installing to an arbitrary --prefix.
  - performance-patched binutils 2.20.1 (from 2.17)
  - xerces 3.1.1 (from 2.8)
  - libmonitor r116
```

## 4.9.9 - 2009-08-29

```
- Rewrite hpcrun (nee csprof)
- Support monitoring statically linked applications using hpclink
- Rename hpcrun to hpcrun-flat
- Rewrite hpcstruct (nee bloop)
- Rewrite hpcprof (nee xcsprof)
- Rewrite hpcprof-flat (nee hpcview, hpcquick, hpcprof)
- Rewrite hpcviewer
- Rewrite externals manager
- Move source code to SciDAC Outreach
- Web page: www.hpctoolkit.org
```

## 4.2.1 - 2006-06-17

```
- Use binutils 2.16.1
```

## 4.2.0 - 2006-04-07

```
- Add csprof to source tree
- Integrate separate version of xcsprof into source tree
- Integrate csprof viewer into source tree
```

## 4.1.3 - 2006-01-09

```
- Split code for hpcrun and hpcprof.
- Automake 1.9.5; Libtool 1.5.20 (still Autoconf 2.59)
```

## 4.1.2 - 2005-08-15

```
- Merge 'lump' (load module dumper) into source tree.  Build when
  configured with --enable-devtools.
- Automake 1.9.5; Libtool 1.5.18 (still Autoconf 2.59)
```

## 4.1.1 - 2005-04-21

```
- Add support for Group scopes
- hpcquick produces hpcviewer output by default
```

## 4.1.0 - 2005-03-18

```
- HPCToolkitRoot
  - Improve sub-repository checkout
  - Improve building by determining and propagating compiler
    optimization and develop options when building external
    repositories (OA, xerces, binutils)
- OpenAnalysis is now 'NewOA'
```

## 4.0.5 - 2005-01-20

```
- Binutils performance tuning:
  - Replace binutils' DWARF2 linear-time line-lookup algorithm with
    binary search.
  - Use a one-element cache to drastically speed up ELF symbol table
    function search
- Update libtool, autoconf and automake to fix build problems on
  IRIX64 (linking templates), Tru64 (including templates in libtool
  archives), and Linux (missing .so).
- Upgrade xercesc from 2.3.0 to 2.6.0
- hpcrun/hpcquick: minor fixes
```

## 4.0.0 - 2004-11-06

```
- Create HPCToolkitRoot, a shell repository to make obtaining sources,
  building and installation easier.
- Autoconf HPCToolkitRoot and hpcviewer.
- HPCToolkit now uses libtool to build libraries.
- Revamp launching of hpcview et al. to not be dependent upon
  Sourcemes.  All needed environment variables are set dynamically.
- Overhaul hpcquick to canonicalize all performance data files into
  PROFILE files before extracting metrics.  Include support for processing
  hpcrun files.
- Merge separate hpcrun/hpcprof into HPCToolkit code base.
- Extend hpcrun:
  - introduce support for profiling statically linked applications
  - create profiles of multiple PAPI or native events
  - monitor POSIX threads
  - follow forks
  - profile through execs
  - create WALLCLK profiles
- Fix bugs in hpcprof:
  - When no line information could be found, samples were dropped
  - Fix several type-size problems.
- bloop:
  - processes DSOs
  - recognize one-bundle loops on IA64 (PC-relative target is 0)
  - classify return instructions in IA64, x86 and Sparc ISAs classify,
    ensuring that in CFG construction no fallthru edge is placed
    between the return and possible subsequent (error handlin) code.
    On Itanium with Intel's compiler, this can have drastic effects.
    Make necessary changes to GNU binutils to propagate this information.
  - Add options to treat irreducible intervals as loops and to turn
    off potentially unsafe normalizations.
- hpcview and hpcquick handle multiple structure files
- Port HPCToolkit to opteron-Linux.
- Update documentation accordingly.
```

## 3.7.0 - 2004-03-12

```
- Replace make system with Autoconf/Automake make system
- Update code so it can be compiled by GCC 3.3.2
- hpcview:
    - Use of hpcviewer is now preferred method for viewing data (as
      opposed to static HTML database).
- bloop:
    - Enable support for Sun's WorkShop/Forte/ONE compiler
    - Revamp scope tree builder
    - Rewrite key normalization routine (coalesce duplicate statements)
    - Enable support of long option switches; improve option parsing.
- xprof:
    - Enable reading of profile data from stdin or file
    - Add basic support for processing Alpha relocated shared libraries
    - Enable support of long option switches; improve option parsing.
```

## 3.6.0 - 2003-07-05

```
- Extend xprof to compute derived metrics from DCPI profiles.  In the
  process, significantly revise/rewrite most existing xprof code and
  extend ISA and AlphaISA class.
- Significantly revise hpcquick to accept PROFILE files with -P option.
- Fix various hpcview bugs and use new xerces SAX interface.
- Revamp HPCToolkit make system (portions of the source tree can easily
  be removed without breaking the build).
- Remove OpenAnalysis, binutils and xercesc from source tree
- Convert OpenAnalysys' make system to Autoconf/make.
- Add front end make system for binutils and xercesc.
- Update HPCToolkit tests for ISA changes and new xprof.
- Update to binutils 2.13.92 (snapshot) and then 2.14 (official release).
- Update to xercesc 2.3.0.
```

## 3.5.2 - 2003-03-28

```
- Rename from HPCTools to HPCToolkit to distinguish from others' use of
  the name.
- Convert from RCS to CVS.
```

## 3.5.1 - 2003-03-07

```
- Update PGM and PROFILE formats to support load modules; other minor
  tweaks.  Update hpcview, bloop, xprof and ptran to use the new formats.
- Add initial DSO support to LoadModule classes.
- Test updates
  - Add LoadModule library tester.
  - Update support library tests.
- Add filter script for f90 modules (f90modfilt).
- Miscellaneous tweaks
  - Fix strcpy bug in GetDemangledFuncName().
  - Makefile tweaks
  - Convert ArchIndType.h limits from const (static) to #defines.
  - Make trace a global variable so that tracing can be globally
    switched on/off.
  - Update alpha macros to support alpha GCC compiler
  - hpcquick now supports recursive paths to option -I.
```

## 3.5.0 - 2003-02-24

```
- Merge HPCView 3.1 and HPCTools 1.20 into one distribution
  - eliminate code duplication (support library, DTDs)
  - port HPCView to ANSI/ISO style C includes (<cheader>; all functions in
    std namespace
  - unify and improve documentation
- Improve make system (e.g., each library is now built in a separate
  location).
- Improve code organization (rename 'libs' to 'lib'; rename and cleanup
  bloop's scope tree files; move general types files to 'src/include')
- Improve and test hvprof with PAPI 2.3.1
```

## 1.2.0 - 2002-10-11

```
----------------------------------------
HPCTools Version 1.20: (bloop 1.20, xprof 1.20) [2002.10.11]
----------------------------------------

- Add xprof test suite.
- Make minor changes to support GCC 3.2.
- Rewrite GNU binutils patch (for dwarf2.c) that handles the
  out-of-order line sequences of Intel's 6.0 compiler.  (The patch is
  faster, slightly more accurate, and makes GNU happy.)
- Change the method of preventing conflict between GNU and Sun's demangler.
```

## 1.1.0 - 2002-08-30

```
----------------------------------------
HPCTools Version 1.10: (bloop 1.10, xprof 1.10) [2002.08.30]
----------------------------------------

- Allow for use of either old-style C headers (<header.h>) or new
  ANSI/ISO style (<cheader>; all functions are in std namespace).  The
  new style is now default.
- Improve error and exception handling.  Detect memory allocation errors.
- Fix bug in GNU-binutils ECOFF reader.
```

## 1.0.5 - 2002-08-23

```
----------------------------------------
HPCTools Version 1.05: (bloop 1.05, xprof 1.05) [2002.08.23]
----------------------------------------

- Update to use binutils-2.13
- Extend VLIW interface throughout HPCTools. (Impose explicit pc +
  operation index interface for instructions.  Now, many comments are
  not lies!)
- Miscellaneous fixes and cleanup.
```

## 1.0.0 - 2002-08-16

```
----------------------------------------
HPCTools Version 1.00: (bloop 1.00, xprof 1.00) [2002.08.16]
----------------------------------------

- Major revisions:
  - Replace EEL binary support with new binutils library (uses GNU's
    binutils) and new ISA library
  - bloop: Replace EEL analysis with OpenAnalysis
  - bloop: Add two new targets (i686-Linux, ia64-Linux) and improve support
    for existing targets (alpha-OSF1, mips-IRIX64, sparc-SunOS);
  - Support 'cross target' processing
- Miscellaneous fixes and cleanup.
  - bloop: Use both system and GNU demangler in demangling attempts
- Update to use binutils-2.12
  - Update to read (abnormal) Compaq ECOFF debugging info.
  - Update to read (abnormal) SGI -64 DWARF2 and g++ -64 DWARF2 debugging
    info.
  - Update to read (abnormal) Intel DWARF2 debugging info
  - Misc. updates
- Reorganize HPCTools directory tree
- Remove (outdated) backwards compatibility for non-standard STL headers

- Add hvprof to HPCTools/tools/hvprof
```

## 0.9.0 - 2001-09

```
----------------------------------------
HPCTools Version 0.90 (bloop 0.90, xprof 0.90) [2001.09]
----------------------------------------

- Port to alpha-OSF1
  - Port EEL to alpha
  - Fix binutils 2.10 ECOFF support
- Add xprof tool (beta) for processing Compaq dcpi output
- Replace EEL dominator analysis with tarjan analysis
- Improve PGM scope tree normalization
- Bring code into compliance with ANSI/ISO C++
  - Remove STLPort and use standard STL
- Bug fixes
```

## 0.8.0 - 2001-02-01

```
----------------------------------------
HPCTools Version 0.80 [2001.02]
----------------------------------------

- Port to mips-IRIX64
  - Port EEL to IRIX64
  - Fix binutils 2.10 DWARF2 support
- Bug fixes
```

## 0.4.0

```
----------------------------------------
HPCTools Initial Version
----------------------------------------
- Support for processing sparc-SunOS binaries compiled with GCC
  - Use EEL to read binaries and find loops
- Update EEL to use binutils 2.10
```

## 0.3.1 - 2003-01-01

```
----------------------------------------
HPCView Version 3.1: [2003.01]
----------------------------------------

- Port to ia64-Linux
- Generated HTML/CSS and JavaScript now works with
  Internet Explorer 5.0+, Netscape 4.x, Netscape 6.2+, or Mozilla 1.0+.
  With one minor exception (in order to support Netscape 4.x) hpcview
  generates valid HTML 4.0 Transitional/Frameset and CSS1/2 style
  sheets.
- Update to use Xerces 2.1.0
```

## 0.3.0

```
----------------------------------------
HPCView Version 3.0 []
----------------------------------------

- This is a major revision to add the option of emitting a compact,
  XML-formatted representation of internal data representation
  ("scope tree") so that it can be efficiently processed by
  (1) HPCViewer, a stand-alone performance data browser (to be
      released real soon now).
  or (2) other down-stream analysis tools.
  The original static HTML output is still supported.

- Almost all of the command line switches have changed. Type "hpcview"
  (no arguments) for command line documentation.
- More run-time assertion exits have been replaced with informative
  error messages.
- Update to use Xerces 1.7.0
```

## 0.2.1

```
----------------------------------------
HPCView Version 2.01 []
----------------------------------------

- Fixed bugs in support for flattening that caused some files to be omitted.
- Regularized error handling for metrics so that file and computed metric
  errors are reported uniformly.
- Added -w option and omitted warnings for file synopsis generation at
  default warning level.
- Fixed error handling so that errors caused by missing file metric data
  or missing inputs to computed metrics (among other metric errors) cause
  an error message to be printed and execution to halt.
- Separated out and removed dead code.
```

## 0.2.0

```
----------------------------------------
HPCView Version 2.0 []
----------------------------------------

- Added support for incorporating program structure information
  generated by a binary analyzer. This enables the hierarchial scopes
  display to show loop-level information. The binary analyzer is
  language independent so this works for any programming language
  whose compiler generates standard symbol table information.
- Hierarchical scopes display now supports "flattening" which allows one to
  compare not only children of a scope, but grand children, great
  grandchildren, etc. This allows one to view a program as composed as
  a set of files, procedures, loops, or (when completely flattened) a
  set of non-loop statements.
- Source pane selections now navigate hierarchical scopes display to
  display the selection.
- Performance table eliminated because it is subsumed by the
  hierarchical scopes display with flattening.
- Generated HTML and JavaScript now works with Internet Explorer 5.5
  as well as Netscape Navigator 4.x.
- Sources adjusted to use standard STL.
```

[2016.12]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/release-2016.12
[2017.06]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/release-2017.06
[2019.08]: https://gitlab.com/hpctoolkit/hpctoolkit/-/commits/6ea44ed3f93ede2d0a48937f288a2d41188a277c
[2021.03.01]: https://gitlab.com/hpctoolkit/hpctoolkit/-/commits/68a051044c952f0f4dac459d9941875c700039e7
[2021.05.15]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/release-2021.05
[2022.01.15]: https://gitlab.com/hpctoolkit/hpctoolkit/-/commit/0238e9a052a696707e4e65b2269f342baad728ae
[2022.04.15]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/release-2022.04
[2022.05.15]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/release-2022.05.15
[2022.10.01]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/release-2022.10.01
[2023.03.01]: https://gitlab.com/hpctoolkit/hpctoolkit/-/tags/2023.03.01
[2023.08.0]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/2023.08.0
[2024.01.1]: https://gitlab.com/hpctoolkit/hpctoolkit/-/releases/2024.01.1
