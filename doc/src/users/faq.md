<!--
SPDX-FileCopyrightText: 2002-2023 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

# FAQ and Troubleshooting

To measure an application's performance with HPCToolkit, one must add
HPCToolkit's measurement subsystem to an application's address
space.

- For a statically-linked binary, one adds HPCToolkit's
  measurement subsystem directly into the binary
  by prefixing your link command
  with HPCToolkit's `hpclink` command.

- For a dynamically-linked
  binary, launching your application with HPCToolkit's `hpcrun`
  command pre-loads HPCToolkit's measurement subsystem into your
  application's address space before the application begins to execute.

In this chapter, for convenience, we refer to HPCToolkit's measurement
system simply as `hpcrun` since the measurement subsystem is most commonly used
with dynamically-linked binaries. From the context, it should be clear enough
whether we are talking about HPCToolkit's measurement subsystem
or the `hpcrun` command itself.

## Instrumenting Statically-linked Applications

### Using `hpclink` with `cmake`

When creating a statically-linked executable with `cmake`, it is not obvious how to add `hpclink` as a prefix to a link command. Unless it is overridden somewhere along the way, the following rule found in `Modules/CMakeCXXInformation.cmake` is
used to create the link command line for a C++ executable:

```cmake
if(NOT CMAKE_CXX_LINK_EXECUTABLE)
  set(CMAKE_CXX_LINK_EXECUTABLE
      "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS>
                            <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
endif()
```

As the rule shows, by default, the C++ compiler is used to link C++ executables. One way to change this is to override the definition for `CMAKE_CXX_LINK_EXECUTABLE` on the `cmake` command line so that it includes the necessary `hpclink` prefix, as shown below:

```bash
cmake srcdir ... \
    -DCMAKE_CXX_LINK_EXECUTABLE="hpclink <CMAKE_CXX_COMPILER> \
        <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> \
        <LINK_LIBRARIES>" ...
```

If your project has executables linked with a C or Fortran compiler, you will need analogous redefinitions for `CMAKE_C_LINK_EXECUTABLE` or `CMAKE_Fortran_LINK_EXECUTABLE` as well.

Rather than adding the redefinitions of these linker rules to the `cmake` command line,
you may find it more convenient to add definitions of these rules to your `CMakeLists.cmake` file.

## General Measurement Failures

### Unable to find HPCTOOLKIT root directory

On some systems, you might see a message like this:

> ```
> /path/to/copy/of/hpcrun: Unable to find HPCTOOLKIT root directory.
> Please set HPCTOOLKIT to the install prefix, either in this script,
> or in your environment, and try again.
> ```

The problem is that the system job launcher copies the `hpcrun`
script from its install directory to a launch directory and runs
it from there. When the system launcher moves `hpcrun` to a different directory, this
breaks `hpcrun`'s method for finding its own install directory.
The solution is to add `HPCTOOLKIT` to your environment so that
`hpcrun` can find its install directory. See section [5.7](#sec:env-vars) for
general notes on environment variables for `hpcrun`. Also, see section [5.8](#sec:platform-specific),
as this problem occurs on Cray XE and XK systems.

Note: Your system may have a module installed for `hpctoolkit` with the
correct settings for `PATH`, `HPCTOOLKIT`, etc. In that case,
the easiest solution is to load the `hpctoolkit` module. If there is
such a module, Try
"`module show hpctoolkit`" to see if it sets `HPCTOOLKIT`.

### Profiling `setuid` programs

`hpcrun` uses preloaded shared libraries to initiate profiling. For this
reason, it cannot be used to profile `setuid` programs.

### Problems loading dynamic libraries

By default, hpcrun uses Glibc's `LD_AUDIT` subsystem to monitor an application's use of dynamic
libraries. Use of `LD_AUDIT` is needed to properly track loaded libraries when a
`RUNPATH` is set in the application or libraries.
Due to known bugs in Glibc's implementation, this may cause the application to crash unexpectedly.
See Section [5.1.1.1](#hpcrun-audit) for details on the issues present and how to avoid them.

### Problems caused by `gprof` instrumentation

When an application has been compiled with the compiler flag `-pg`,
the compiler adds instrumentation to collect performance measurement data for
the `gprof` profiler. Measuring application performance with
HPCToolkit's measurement subsystem and `gprof` instrumentation
active in the same execution may cause the execution
to abort. One can detect the presence of `gprof` instrumentation in an
application by the presence of the `__monstartup` and `_mcleanup` symbols
in a executable. You can recompile your code without the
`-pg` compiler flag and measure again. Alternatively, you can use the `--disable-gprof`
argument to `hpcrun` or `hpclink` to disable `gprof` instrumentation while
measuring performance with HPCToolkit.

To cope with `gprof` instrumentation in dynamically-linked programs, you can use `hpcrun`'s `--disable-gprof` option.

## Measurement Failures using NVIDIA GPUs

### Deadlock while monitoring a program that uses IBM Spectrum MPI and NVIDIA GPUs

IBM's Spectrum MPI uses a special library `libpami_cudahook.so` to intercept allocations of GPU memory so that Spectrum MPI knows when data is allocated on an NVIDIA GPU.
Unfortunately, the mechanism used by Spectrum MPI to do so (wrapping `dlsym`) interferes with performance tools that use `dlopen` and `dlsym`.
This interference causes measurement of a GPU-accelerated MPI application using HPCToolkit to deadlock when an application uses both Spectrum MPI and and CUDA on an NVIDIA GPU.

To avoid this deadlock on systems when launching a program that uses Spectrum MPI with `jsrun`, use `--smpiargs="-x PAMI_DISABLE_CUDA_HOOK=1 -disable_gpu_hooks"` to disable the PAMI CUDA hook library.
These flags cannot be used with the `-gpu` flag.

Note however that disabling Spectrum MPI's CUDA hook will cause trouble if CUDA device memory is passed into the MPI library as a send or receive buffer.
An additional restriction is that memory obtained with a call to `cudaMallocHost` may not be passed as a send or receive buffer.
Functionally similar memory may be obtained with any host allocation function followed by a call the `cudaHostRegister`.

### Ensuring permission to use GPU performance counters

Your Administrator or a recent NVIDIA driver installation may have disabled access to GPU Performance due to Security Notice: NVIDIA Response to "Rendered Insecure: GPU Side Channel Attacks are Practical" <https://nvidia.custhelp.com/app/answers/detail/a_id/4738> - November 2018.
If that is the case, HPCToolkit cannot access NVIDIA GPU performance counters when using a Linux 418.43 or later driver. This may cause an error message when you try to use PC sampling on an NVIDIA GPU.

A good way to check whether GPU performance counters are available to non-root users on Linux is to execute the following commands:

1. `cd /etc/modprobe.d`

1. `grep NVreg_RestrictProfilingToAdminUsers *`

Generally, if non-root user access to GPU performance counters is enabled, the `grep` command above should yield a line that contains `NVreg_RestrictProfilingToAdminUsers=0`.
Note: if you are on a cluster, access to GPU performance counters may be disabled on a login node, but enabled on a compute node. You should run an interactive job on a compute node and perform the checks there.

If access to GPU hardware performance counters is not enabled, one option you have is to use `hpcrun` without PC sampling, i.e., with the `-e gpu=nvidia` option instead of `-e gpu=nvidia,pc`.

If PC sampling is a must, you have two options:

1. Run the tool or application being profiled with administrative privileges.
   On Linux, launch HPCToolkit with `sudo` or as a user with the `CAP_SYS_ADMIN` capability set.

1. Have a system administrator enable access to the NVIDIA performance counters using the instructions on the following web page: <https://developer.nvidia.com/ERR_NVGPUCTRPERM>.

### Avoiding the error `cudaErrorUnknown`

When monitoring a CUDA application with `REALTIME` or `CPUTIME`, you may encounter a
`cudaErrorUnknown` return from many or all CUDA calls in the application.
[^18]
This error may occur non-deterministically, we have observed that this error occurs regularly
at very fast periods such as `REALTIME@100`. If this occurs, we recommend using `CYCLES`
as a working alternative similar to `CPUTIME`, see Section [12.4.1](#sec:troubleshooting:hpcrun-sample-periods)
for more detail on HPCToolkit's `perfevents` support.

### Avoiding the error `CUPTI_ERROR_NOT_INITIALIZED`

`hpcrun` uses NVIDIA's CUDA Performance Tools Interface known as
CUPTI to monitor computations on NVIDIA GPUs. In our experience,
this error occurs when the version of CUPTI used by HPCToolkit is
incompatible with the version of CUDA used by your program or CUDA kernel driver installed on your system. You can check the
version of the CUDA kernel driver installed on your system using the `nvidia-smi` command.
Table 3 *CUDA Application Compatibility Support Matrix* at the following URL <https://docs.nvidia.com/deploy/cuda-compatibility/index.html#cuda-application-compatibility>
specifies what versions of the CUDA kernel driver match each version of CUDA and CUPTI.
Although the table indicates that some drivers can support newer versions of CUDA than the one that they were designed for,
e.g. driver 418.40.04+ designed to support CUDA 10.1 can also run CUDA 11.0 and 11.1 programs,
in our experience that does not necessarily mean that the driver will support performance measurement of CUDA programs
using any CUDA version newer than 10.1.
We believe that best way to avoid the `CUPTI_ERROR_NOT_INITIALIZED` error is to ensure that
(1) HPCToolkit is compiled with the version of CUDA that your installed CUDA kernel driver was designed to support, and
(2) your application uses the version of CUDA that matches the one your kernel driver was designed to support or a compatible older version.

### Avoiding the error `CUPTI_ERROR_HARDWARE_BUSY`

When trying to use PC sampling to measure computation on an NVIDIA GPU, you may encounter the following error: 'function `cuptiActivityConfigurePCSampling` failed with error `CUPTI_ERROR_HARDWARE_BUSY`'.

For all versions of CUDA to date (through CUDA 11), NVIDIA's CUPTI library only supports PC sampling for only one process per GPU. If multiple MPI ranks in your application run CUDA on the same GPU, you may see this error.[^19]

You have two alternatives:

1. Measure the execution in which multiple MPI ranks share a GPU using only `-e gpu=nvidia` without PC sampling.

1. Launch your program so that there is only a single MPI rank per GPU.

   1. `jsrun` advice: if using `-g1` for a resource set, don't use anything other than `-a1`.

### Avoiding the error `CUPTI_ERROR_UNKNOWN`

When trying to use PC sampling to measure computation on an NVIDIA GPU, you may encounter the following error: 'function `cuptiActivityEnableContext` failed with error `CUPTI_ERROR_UNKNOWN`'.

For all versions of CUDA to date (through CUDA 11), NVIDIA's CUPTI library only supports PC sampling for only one process per GPU. If multiple MPI ranks in your application run CUDA on the same GPU, you may see this error.[^20] You have two alternatives:

1. Measure the execution in which multiple MPI ranks share a GPU using only `-e gpu=nvidia` without PC sampling.

1. Launch your program so that there is only a single MPI rank per GPU.

   1. `jsrun` advice: if using `-g1` for a resource set, don't use anything other than `-a1`.

## General Measurement Issues

(sec:troubleshooting:hpcrun-sample-periods)=

### How do I choose sampling periods?

When using sample sources for hardware counter and software counter events provided by Linux `perf_events`,
we recommend that you use frequency-based sampling. The default frequency is 300 samples/second.

Statisticians use samples sizes of approximately 3500 to make accurate projections about the voting preferences of millions of people.
In an analogous way, rather than measuring and attributing every action of a program or every runtime event (e.g., a cache miss), sampling-based performance measurement collects "just enough" representative performance data.
You can control `hpcrun`'s sampling periods to collect "just enough" representative data even for very long executions and, to a lesser degree, for very short executions.

For reasonable accuracy (+/- 5%), there should be at least 20 samples in each context that is important with respect to performance.
Since unimportant contexts are irrelevant to performance, as long as this condition is met (and as long as samples are not correlated, etc.), HPCToolkit's performance data should be accurate enough to guide program tuning.

We typically recommend targeting a frequency of hundreds of samples per second.
For very short runs, you may need to collect thousands of samples per second to record an adequate number of samples.
For long runs, tens of samples per second may suffice for performance diagnosis.

Choosing sampling periods for some events, such as Linux timers, cycles and instructions, is easy given a target sampling frequency.
Choosing sampling periods for other events such as cache misses is harder.
In principle, an architectural expert can easily derive reasonable sampling periods by working backwards from (a) a maximum target sampling frequency and (b) hardware resource saturation points.
In practice, this may require some experimentation.

See also the `hpcrun` [man page](http://hpctoolkit.org/man/hpcrun.html).

### Why do I see partial unwinds?

Under certain circumstances, HPCToolkit can't fully unwind the call
stack to determine the full calling context where a sample event
occurred. Most often, this occurs when `hpcrun` tries to unwind
through functions in a shared library or executable that has not
been compiled with `-g` as one of its options. The `-g`
compiler flag can be used in addition to optimization flags. On
Power and `x86_64` processors, `hpcrun` can often compensate
for the lack of unwind recipes by using
binary analysis to compute recipes itself. However, since `hpcrun`
lacks binary analysis capabilities for ARM processors, there is a
higher likelihood that the lack of a `-g` compiler option for
an executable or a shared library will lead to partial unwinds.

One annoying place where partial unwinds are somewhat common on `x86_64`
processors is in Intel's MKL family of libraries. A careful examination of Intel's
MKL libraries showed that most but not all routines have
compiler-generated Frame Descriptor Entries (FDEs) that help tools
unwind the call stack. For any routine that lacks
an FDE, HPCToolkit tries to compensate using binary analysis.
Unfortunately, highly-optimized code in MKL library
routines has code features that are difficult to analyze correctly.

There are two ways to deal with this problem:

- Analyze the execution using information from partial unwinds. Often knowing several levels of calling context is enough for analysis without full calling context for sample events.

- Recompile the binary or shared library causing the problem and add `-g` to the list of its compiler options.

### Measurement with HPCToolkit has high overhead! Why?

For reasonable sampling periods, we expect `hpcrun`'s overhead percentage to be in the low single digits, e.g., less than 5%.
The most common causes for unusually high overhead are the following:

- Your sampling frequency is too high.
  Recall that the goal is to obtain a representative set of performance data.
  For this, we typically recommend targeting a frequency of hundreds of samples per second.
  For very short runs, you may need to try thousands of samples per second.
  For very long runs, tens of samples per second can be quite reasonable.
  See also Section [12.4.1](#sec:troubleshooting:hpcrun-sample-periods).

- `hpcrun` has a problem unwinding.
  This causes overhead in two forms.
  First, `hpcrun` will resort to more expensive unwind heuristics and possibly have to recover from self-generated segmentation faults.
  Second, when these exceptional behaviors occur, `hpcrun` writes some information to a log file.
  In the context of a parallel application and overloaded parallel file system, this can perturb the execution significantly.
  To diagnose this, execute the following command and look for "Errant Samples":

  > `hpcsummary --all <hpctoolkit-measurements>`

  Note: The `hpcsummary` script is no longer included in the `bin` directory of an HPCToolkit installation;
  it is a developer script that can be found in the `libexec/hpctoolkit` directory.
  Let us know if you encounter significant problems with bad unwinds.

- You have very long call paths where long is in the hundreds or thousands.
  On x86-based architectures, try additionally using `hpcrun`'s `RETCNT` event.
  This has two effects: It causes `hpcrun` to collect function return counts and to memoize common unwind prefixes between samples.

- Currently, on very large runs the process of writing profile data can take a long time.
  However, because this occurs after the application has finished executing, it is relatively benign overhead.
  (We plan to address this issue in a future release.)

### Some of my syscalls return EINTR

When profiling a threaded program, there are times when it is
necessary for `hpcrun` to signal another thread to take some action.
When this happens, if the thread receiving the signal is blocked in a
syscall, the kernel may return EINTR from the syscall. This would
happen only in a threaded program and mainly with "slow" syscalls
such as `select()`, `poll()` or `sem_wait()`.

### My application spends a lot of time in C library functions with names that include `mcount`

If performance measurements with HPCToolkit show that your
application is spending a lot of time in C library routines with
names that include the string `mcount` (e.g., `mcount`, `_mcount` or
`__mcount_internal`), your code has been compiled with the compiler
flag `-pg`, which adds instrumentation to collect performance
measurement data for the `gprof` profiler. If you are using
HPCToolkit to collect performance data, the `gprof` instrumentation
is needlessly slowing your application. You can recompile your code without the
`-pg` compiler flag and measure again. Alternatively, you can use the `--disable-gprof`
argument to `hpcrun` or `hpclink` to disable `gprof` instrumentation while
measuring performance with HPCToolkit.

(section:hpcstruct-cubin)=

## Problems Recovering Loops in NVIDIA GPU binaries

- When using the `--gpucfg yes` option to analyze control flow to recover information about loops in CUDA binaries, `hpcstruct` needs to use NVIDIA's `nvdisasm` tool. It is important to note that `hpcstruct` uses the version of `nvdisasm` that is on your path. When using the `--gpucfg yes` option to recover loops in CUBINs, you can improve `hpcstruct`'s ability to recover loops by having a newer version of `nvdisasm` on your path. Specifically, the version of `nvdisasm` in CUDA 11.2 is much better than `nvdisasm` in CUDA 10.2. It will recover loops for more procedures and faster.

- While NVIDIA has improved the capability and speed of `nvdisasm` in CUDA 11.2, it may still be too slow to be usable on large CUDA binaries. Because of failures we have encountered with `nvdisasm`, `hpcstruct` launches `nvdisasm` once for each procedure in a GPU binary to maximize the information it can extract. With this approach, we have seen `hpcstruct` take over 12 hours to analyze a CUBIN of roughly 800MB with 40K GPU functions. For large CUDA binaries, our advice is to skip the `--gpucfg yes` option at present until we adjust `hpcstruct` launch multiple copies of `nvdisasm` in parallel to reduce analysis time.

## Graphical User Interface Issues

### Fail to run `hpcviewer`: executable launcher was unable to locate its companion shared library

Although this error mostly incurrs on Windows platform, but it can happen in other environment.
The cause of this issue is that the permission of one of Eclipse launcher library (org.eclipse.equinox.launcher.\*) is too restricted.
To fix this, set the permission of the library to 0755, and launch again the viewer.

### Launching `hpcviewer` is very slow on Windows

There is a known issue that Windows Defender significantly slow down Java-based applications.
See the github issue at <https://github.com/microsoft/java-wdb/issues/9>.

A temporary solution is to add `hpcviewer` in the Windows' exclusion list:

1. Open Windows 10 settings.

1. Search for "Virus and threat protection" and open it.

1. Now click on "Manage settings" under "Virus and threat protection settings" section.

1. Now click "Add or remove exclusions" under "Exclusions" section.

1. Now click "Add an exclusion" then select "Folder"

1. Point to `hpcviewer` directory and press "Select Folder"

### Mac only: `hpcviewer` runs on Java X instead of "Java 11"

`hpcviewer` has mainly been tested on Java 11. If you are running an older than Java 11 or newer than Java 17, obtain a version of Java 11 or 17 from <https://adoptopenjdk.net> or <https://adoptium.net/>.

If your system has multiple versions of Java and Java 11 is not the newest version, you need to set Java 11 as the default JVM. On MacOS, you need to exclude older Java as follows:

1. Leave all JDKs at their default location (usually under `/Library/Java/JavaVirtualMachines`). The system will pick the highest version by default.

1. To exclude a JDK from being picked by default, rename `Contents/Info.plist` file to other name like `Info.plist.disabled`. That JDK can still be used when `$JAVA_HOME` points to it, or explicitly referenced in a script or configuration. It will simply be ignored by your Mac's java command.

### When executing `hpcviewer`, it complains cannot create "Java Virtual Machine"

If you encounter this problem, we recommend that you edit the
`hpcviewer.ini`
file which is located in HPCToolkit installation directory to reduce the
Java heap size.
By default, the content of the file on Linux `x86` is as follows:

```
-startup
plugins/org.eclipse.equinox.launcher_1.6.200.v20210416-2027.jar
--launcher.library
plugins/org.eclipse.equinox.launcher.gtk.linux.x86_64_1.2.200.v20210429-1609
-clearPersistedState
-vmargs
-Xmx2048m
-Dosgi.locking=none
```

You can decrease the maximum size of the Java heap from 2048MB to 1GB
by changing the `Xmx` specification in the `hpcviewer.ini` file as follows:

```
-Xmx1024m
```

### `hpcviewer` fails to launch due to `java.lang.NoSuchMethodError` exception.

The root cause of the error is due to a mix of old and new hpcviewer binaries.
To solve this problem, you need to remove your hpcviewer workspace (usually in your `$HOME/.hpctoolkit/hpcviewer` directory), and run `hpcviewer` again.

### `hpcviewer` fails due to `java.lang.OutOfMemoryError` exception.

If you see this error, the memory footprint that `hpcviewer` needs to store and the metrics for your measured program execution exceeds the maximum size for the Java heap specified at program launch. On Linux, `hpcviewer` accepts a command-line option `--java-heap` that enables you to specify a larger non-default value for the maximum size of the Java heap. Run `hpcviewer --help` for the details of how to use this option.

### `hpcviewer` writes a long list of Java error messages to the terminal!

The Eclipse Java framework that serves as the foundation for `hpcviewer` can be somewhat temperamental. If the persistent state maintained by Eclipse for `hpcviewer`
gets corrupted, `hpcviewer` may spew a list of errors deep within call chains of the Eclipse framework.

On MacOS and Linux, try removing your `hpcviewer` Eclipse workspace with default location `$HOME/.hpctoolkit/hpcviewer` and run `hpcviewer` again.

(sec:troubleshooting:debug-info)=

### `hpcviewer` attributes performance information only to functions and not to source code loops and lines! Why?

Most likely, your application's binary either lacks debugging information or is stripped.
A binary's (optional) debugging information includes a line map that is used by profilers and debuggers to map object code to source code.
HPCToolkit can profile binaries without debugging information, but without such debugging information it can only map performance information (at best) to functions instead of source code loops and lines.

For this reason, we recommend that you always compile your production applications with optimization *and* with debugging information.
The options for doing this vary by compiler.
We suggest the following options:

- GNU compilers (`gcc`, `g++`, `gfortran`): `-g`

- IBM compilers (xlc, xlf, xlC): `-g`

- Intel compilers (`icc`, `icpc`, `ifort`): `-g -debug inline_debug_info`

- PGI compilers (`pgcc`, `pgCC`, `pgf95`): `-gopt`.

We generally recommend adding optimization options *after* debugging options --- e.g., '`-g -O2`' --- to minimize any potential effects of adding debugging information.
[^21]
Also, be careful not to strip the binary as that would remove the debugging information.
(Adding debugging information to a binary does not make a program run slower; likewise, stripping a binary does not make a program run faster.)

Please note that at high optimization levels, a compiler may make significant program transformations that do not cleanly map to line numbers in the original source code.
Even so, the performance attribution is usually very informative.

### `hpcviewer` hangs trying to open a large database! Why?

The most likely problem is that the Java virtual machine is low on memory and thrashing. The memory footprint that `hpcviewer` needs to store and the metrics for your measured program execution is likely near the maximum size for the Java heap specified at program launch.

On Linux, `hpcviewer` accepts a command-line option `--java-heap` that enables you to specify a larger non-default value for the maximum size of the Java heap. Run `hpcviewer --help` for the details of how to use this option.

### `hpcviewer` runs glacially slowly! Why?

There are three likely reasons why `hpcviewer` might run slowly.
First, you may be running `hpcviewer` on a remote system with low bandwidth, high latency or an otherwise unsatisfactory network connection to your desktop.
If any of these conditions are true, `hpcviewer`'s otherwise snappy GUI can become sluggish if not downright unresponsive.
The solution is to install `hpcviewer` on your local system, copy the database onto your local system, and run `hpcviewer` locally.
We almost always run `hpcviewer` on our local desktops or laptops for this reason.

Second, the HPCToolkit database may be very large, which can cause the Java virtual machine to run short on memory and thrash.
The memory footprint that `hpcviewer` needs to store and the metrics for your measured program execution is likely near the maximum size for the Java heap specified at program launch.
On Linux, `hpcviewer` accepts a command-line option `--java-heap` that enables you to specify a larger non-default value for the maximum size of the Java heap. Run `hpcviewer --help` for the details of how to use this option.

### `hpcviewer` does not show my source code! Why?

Assuming you compiled your application with debugging information (see Issue [12.6.8](#sec:troubleshooting:debug-info)),
the most common reason that `hpcviewer` does not show source code is that `hpcprof/mpi`
could not find it and therefore could not copy it into the HPCToolkit performance database.

#### Follow 'best practices'

When running `hpcprof/mpi`, we recommend using an `-I/--include` option to specify a search directory for each distinct top-level source directory (or build directory, if it is separate from the source directory).
Assume the paths to your top-level source directories are `<dir1>` through `<dirN>`.
Then, pass the the following options to `hpcprof/mpi`:

> `-I <dir1>/+ -I <dir2>/+ ... -I <dirN>/+`

These options instruct `hpcprof/mpi` to search for source files that live within any of the source directories `<dir1>` through `<dirN>`.
Each directory argument can be either absolute or relative to the current working directory.

It will be instructive to unpack the rationale behind this recommendation.
`hpcprof/mpi` obtains source file names from your application binary's debugging information.
These source file paths may be either absolute or relative.
Without any `-I/--include` options, `hpcprof/mpi` can find source files that either (1) have absolute paths (and that still exist on the file system) or (2) are relative to the current working directory.
However, because the nature of these paths depends on your compiler and the way you built your application, it is not wise to depend on either of these default path resolution techniques.
For this reason, we always recommend supplying at least one `-I/--include` option.

There are two basic forms in which the search directory can be specified: non-recursive and recursive.
In most cases, the most useful form is the recursive search directory, which means that the directory should be searched *along with all of its descendants*.
A non-recursive search directory `dir` is simply specified as `dir`.
A recursive search directory `dir` is specified as the base search directory followed by the special suffix '`/+`': `dir/+`.
The paths above use the recursive form.

#### An explanation how HPCToolkit finds source files

`hpcprof/mpi` obtains source file names from your application binary's debugging information.
If debugging information is unavailable, such as is often the case for system or math libraries, then source files are unknown.

Two things immediately follow from this.
First, in most normal situations, there will always be some functions for which source code cannot be found, such as those within system libraries.[^22]
Second, to ensure that `hpcprof/mpi` has file names for which to search, make sure as much of your application as possible (including libraries) contains debugging information.

If debugging information is available, source files can come in two forms: absolute and relative.
`hpcprof/mpi` can find source files under the following conditions:

- If a source file path is absolute and the source file can be found on the file system, then `hpcprof/mpi` will find it.

- If a source file path is relative, `hpcprof/mpi` can only find it if the source file can be found from the current working directory or within a search directory (specified with the `-I/--include` option).

- Finally, if a source file path is absolute and cannot be found by its absolute path, `hpcprof/mpi` uses a special search mode.
  Let the source file path be `p/f`.
  If the path's base file name `f` is found within a search directory, then that is considered a match.
  This special search mode accommodates common complexities such as:
  (1) source file paths that are relative not to your source code tree but to the directory where the source was compiled;
  (2) source file paths to source code that is later moved; and
  (3) source file paths that are relative to file system that is no longer mounted.

Note that given a source file path `p/f` (where `p` may be relative or absolute), it may be the case that there are multiple instances of a file's base name `f` within one search directory, e.g., `p_1/f` through `p_n/f`, where `p_i` refers to the `i`th path to `f`.
Similarly, with multiple search-directory arguments, `f` may exist within more than one search directory.
If this is the case, the source file `p/f` is resolved to the first instance `p'/f` such that `p'` best corresponds to `p`, where instances are ordered by the order of search directories on the command line.

For any functions whose source code is not found (such as functions within system libraries), `hpcviewer` will generate a synopsis that shows the presence of the function and its line extents (if known).

### `hpcviewer`'s reported line numbers do not exactly correspond to what I see in my source code! Why?

To use a cliché, "garbage in, garbage out".
HPCToolkit depends on information recorded in the symbol table by the compiler.
Line numbers for procedures and loops are inferred by looking at the symbol table information recorded for machine instructions identified as being inside the procedure or loop.

For procedures, often no machine instructions are associated with a procedure's declarations.
Thus, the first line in the procedure that has an associated machine instruction is the first line of executable code.

Inlined functions may occasionally lead to confusing data for a procedure.
Machine instructions mapped to source lines from the inlined function appear in the context of other functions.
While `hpcprof`'s methods for handling incline functions are good, some codes can confuse the system.

For loops, the process of identifying what source lines are in a loop is similar to the procedure process: what source lines map to machine instructions inside a loop defined by a backward branch to a loop head.
Sometimes compilers do not properly record the line number mapping.

### `hpcviewer` claims that there are several calls to a function within a particular source code scope, but my source code only has one! Why?

In the course of code optimization, compilers often replicate code blocks.
For instance, as it generates code, a compiler may peel iterations from a loop or split the iteration space of a loop into two or more loops.
In such cases, one call in the source code may be transformed into multiple distinct calls that reside at different code addresses in the executable.

When analyzing applications at the binary level, it is difficult to determine whether two distinct calls to the same function that appear in the machine code were derived from the same call in the source code.
Even if both calls map to the same source line, it may be wrong to coalesce them; the source code might contain multiple calls to the same function on the same line.
By design, HPCToolkit does not attempt to coalesce distinct calls to the same function because it might be incorrect to do so; instead, it independently reports each call site that appears in the machine code.
If the compiler duplicated calls as it replicated code during optimization, multiple call sites may be reported by `hpcviewer` when only one appeared in the source code.

### Trace view shows lots of white space on the left. Why?

At startup, Trace view renders traces for the time interval between the minimum and maximum times recorded for any process or thread in the execution. The minimum time for each process or thread is recorded when its trace file is opened as HPCToolkit's monitoring facilities are initialized at the beginning of its execution. The maximum time for a process or thread is recorded when the process or thread is finalized and its trace file is closed. When an application uses the `hpctoolkit_start` and `hpctoolkit_stop` primitives, the minimum and maximum time recorded for a process/thread are at the beginning and end of its execution, which may be distant from the start/stop interval. This can cause significant white space to appear in Trace view's display to the left and right of the region (or regions) of interest demarcated in an execution by start/stop calls.

## Debugging

### How do I debug HPCToolkit's measurement?

Assume you want to debug HPCToolkit's measurement subsystem when
collecting measurements for an application named `app`.

### Tracing `libmonitor`

HPCToolkit's measurement subsystem
uses `libmonitor` for process/thread control.
To collect a debug trace of `libmonitor`, use either `monitor-run` or `monitor-link`, which are located within:

> `<externals-install>/libmonitor/bin`

Launch your application as follows:

- Dynamically linked applications:

  > `[<mpi-launcher>] monitor-run --debug app [app-arguments]`

- Statically linked applications:

  Link `libmonitor` into `app`:

  > `monitor-link <linker> -o app <linker-arguments>`

  Then execute `app` under special environment variables:

  > ```
  > export MONITOR_DEBUG=1
  > [<mpi-launcher>] app [app-arguments]
  > ```

### Tracing HPCToolkit's Measurement Subsystem

Broadly speaking, there are two levels at which a user can test `hpcrun`.
The first level is tracing `hpcrun`'s application control, that is, running `hpcrun` without an asynchronous sample source.
The second level is tracing `hpcrun` with a sample source.
The key difference between the two is that the former uses the `--event NONE` or `HPCRUN_EVENT_LIST="NONE"` option (shown below) whereas the latter does not (which enables the default CPUTIME sample source).
With this in mind, to collect a debug trace for either of these levels, use commands similar to the following:

- Dynamically linked applications:

  > ```
  > [<mpi-launcher>] \
  >   hpcrun --monitor-debug --dynamic-debug ALL --event NONE \
  >     app [app-arguments]
  > ```

- Statically linked applications:

  Link `hpcrun` into `app` (see Section [3.1.2](#chpt:quickstart:tour:measurement)).
  Then execute `app` under special environment variables:

  > ```
  > export MONITOR_DEBUG=1
  > export HPCRUN_EVENT_LIST="NONE"
  > export HPCRUN_DEBUG_FLAGS="ALL"
  > [<mpi-launcher>] app [app-arguments]
  > ```

Note that the `*debug*` flags are optional.
The `--monitor-debug/MONITOR_DEBUG` flag enables `libmonitor` tracing.
The `--dynamic-debug/HPCRUN_DEBUG_FLAGS` flag enables `hpcrun` tracing.

### Using a debugger to inspect an execution being monitored by HPCToolkit

If HPCToolkit has trouble monitoring an application, you may find it useful to
execute an application being monitored by HPCToolkit under the control
of a debugger to observe how HPCToolkit's measurement subsystem interacts with the application.

HPCToolkit's measurement subsystem is easiest to debug if you configure and
build HPCToolkit by adding the `--enable-develop` option as an argument to `configure` when preparing to build HPCToolkit.
(It is not necessary to rebuild HPCToolkit's `hpctoolkit-externals`.)

One can debug a statically-linked or a dynamically-linked applications being measured by
HPCToolkit's measurement subsystem.

- Dynamically-linked applications. When launching an application with `hpcrun`, add the `--debug` option to `hpcrun`.

- Statically-linked applications. To debug a statically-linked application that has HPCToolkit's measurement subsystem linked into it, set `HPCRUN_WAIT` in the environment before launching the application, e.g.

  ```
  export HPCRUN_WAIT=1
  export HPCRUN_EVENT_LIST="... the metric(s) you want to measure ..."
  app [app-arguments]
  ```

There are two ways to use launch an application with a debugger when using
To attach a debugger when monitoring an application using `hpcrun`, add `hpcrun`'s `--debug` option

o debug hpcrun with a debugger use the following approach.

1. Launch your application.
   To debug `hpcrun` without controlling sampling signals, launch normally.
   To debug `hpcrun` with controlled sampling signals, launch as follows:

   > ```
   > hpcrun --debug --event REALTIME@0 app [app-arguments]
   > ```

   or

   > ```
   > export HPCRUN_WAIT=1
   > export HPCRUN_EVENT_LIST="REALTIME@0"
   > app [app-arguments]
   > ```

1. Attach a debugger.
   The debugger should be spinning in a loop whose exit is conditioned by the
   `HPCRUN_DEBUGGER_WAIT` variable.

1. Set any desired breakpoints.
   To send a sampling signal at a particular point, make sure to stop at that point with a *one-time* or *temporary* breakpoint (`tbreak` in GDB).

1. Call `hpcrun_continue()` or set the `HPCRUN_DEBUGGER_WAIT` variable to 0
   and continue.

1. To raise a controlled sampling signal, raise a SIGPROF, e.g., using GDB's command `signal SIGPROF`.

[^18]: We have observed this error on ORNL's Summit machine, running Red Hat Enterprise Linux 8.2.

[^19]: We have observed this error on CUDA 11.

[^20]: We have observed this error on CUDA 10.

[^21]: In general, debugging information is compatible with compiler optimization.
    However, in a few cases, compiling with debugging information will disable some optimization.
    We recommend placing optimization options *after* debugging options because compilers usually resolve option incompatibilities in favor of the last option.

[^22]: Having a system administrator download the associated `devel` package for a library can enable visibility into the source code of system libraries.
