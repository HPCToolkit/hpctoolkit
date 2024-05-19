<!--
SPDX-FileCopyrightText: 2002-2023 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

# Quick Start

This chapter provides a rapid overview of analyzing the performance of an application using HPCToolkit.
It assumes an operational installation of HPCToolkit.

## Guided Tour

```{figure-md} fig:hpctoolkit-overview:b
![](hpctoolkit-gpu-workflow.png)

Overview of HPCToolkit tool's work flow.
```

HPCToolkit's work flow is summarized in Figure [3.1](#fig:hpctoolkit-overview:b) (on page ) and is organized around four principal capabilities:

1. *measurement* of context-sensitive performance metrics while an application executes;

1. *binary analysis* to recover program structure from CPU and GPU binaries;

1. *attribution* of performance metrics by correlating dynamic performance metrics with static program structure; and

1. *presentation* of performance metrics and associated source code.

To use HPCToolkit to measure and analyze an application's performance, one first compiles and links the application for a production run, using *full* optimization.
Second, one launches an application with HPCToolkit's measurement tool, `hpcrun`, which uses statistical sampling to collect a performance profile.
Third, one applies `hpcstruct` to an application's measurement directory to recover program structure information from any CPU or GPU binary that was measured.
Program structure, which includes information about files, procedures, inlined code, and loops, is used to relate performance measurements to source code.
Fourth, one uses `hpcprof` to combine information about an application's structure with dynamic performance measurements to produce a performance database.
Finally, one explores a performance database with HPCToolkit's graphical user interface: `hpcviewer` which presents
both a code-centric analysis of performance metrics and a time-centric (trace-based) analysis of an execution.

The following subsections explain HPCToolkit's work flow in more detail.

### Compiling an Application

For the most detailed attribution of application performance data using HPCToolkit, one should compile so as to include with line map information in the generated object code.
This usually means compiling with options similar to '`-g -O3`'. Check your compiler's documentation for information about the right set of options to have the compiler record information about inlining and the mapping of machine instructions to source lines. We advise picking options that indicate they will record information that relates machine instructions to source code without compromising optimization. For instance, the Portland Group (PGI) compilers, use `-gopt` in place of `-g` to collect information without interfering with optimization.

While HPCToolkit does not need information about the mapping between machine instructions and source code to function,
having such information included in the binary code by the compiler can be helpful to users trying to interpret performance measurements.
Since compilers can usually provide information about line mappings and inlining for fully-optimized code,
this requirement usually involves a one-time trivial adjustment to the an application's build scripts
to provide a better experience with tools. Such mapping information enables tools such as HPCToolkit,
race detectors, and memory analysis tools to attribute information more precisely.

For statically linked executables, such as those often used on Cray supercomputers, the final link step is done with `hpclink`.

(chpt:quickstart:tour:measurement)=

### Measuring Application Performance

Measurement of application performance takes two different forms depending on whether your application is dynamically or statically linked.
To monitor a dynamically linked application, simply use `hpcrun` to launch the application.
To monitor a statically linked application, the data to be collected is specified by environment variables.
In either case, the application may be sequential, multithreaded or based on MPI.
The commands below give examples for an application named `app`.

- Dynamically linked applications:

  Simply launch your application with `hpcrun`:

  > `[<mpi-launcher>] hpcrun [hpcrun-options] app [app-arguments]`

  Of course, `<mpi-launcher>` is only needed for MPI programs and is sometimes a program like `mpiexec` or `mpirun`, or a workload manager's utilities such as Slurm's `srun` or IBM's Job Step Manager utility `jsrun`.

- Statically linked applications:

  First, link `hpcrun`'s monitoring code into `app`, using `hpclink`:

  > `hpclink <linker> -o app <linker-arguments>`

  Then monitor `app` by passing `hpcrun` options through environment variables.
  For instance:

  > ```
  > export HPCRUN_EVENT_LIST="CYCLES"
  > [<mpi-launcher>] app [app-arguments]
  > ```

  `hpclink`'s `--help` option gives a list of environment variables that affect monitoring.
  See Chapter [6](#chpt:statically-linked-apps) for more information.

Any of these commands will produce a measurements database that contains separate measurement information for each MPI rank and thread in the application.
The database is named according the form:

> `hpctoolkit-app-measurements[-<jobid>]`

If the application `app` is run under control of a recognized batch job scheduler (such as Slurm, Cobalt, or IBM's Job Manager), the name of the measurements directory will contain the corresponding job identifier `<jobid>`.
Currently, the database contains measurements files for each thread that are named using the following templates:

> `app-<mpi-rank>-<thread-id>-<host-id>-<process-id>.<generation-id>.hpcrun`
> `app-<mpi-rank>-<thread-id>-<host-id>-<process-id>.<generation-id>.hpctrace`

#### Specifying CPU Sample Sources

HPCToolkit primarily monitors an application using asynchronous sampling.
Consequently, the most common option to `hpcrun` is a list of sample sources that define how samples are generated.
A sample source takes the form of an event name `e` and `howoften`, specified as `e@howoften`. The specifier `howoften` may
be a number, indicating a period, e.g. `CYCLES@4000001` or it may be `f` followed by a number, `CYCLES@f200` indicating a frequency in samples/second.
For a sample source with event `e` and period `p`, after every `p` instances of `e`, a sample is generated that causes `hpcrun` to inspect the and record information about the monitored application.

To configure `hpcrun` with two samples sources, `e1@howoften1` and `e2@howoften2`, use the following options:

> `--event e1@howoften1 --event e2@howoften2`

To use the same sample sources with an `hpclink`-ed application, use a command similar to:

> `export HPCRUN_EVENT_LIST="e1@howoften1 e2@howoften2"`

#### Measuring GPU Computations

One can simply profile and optionally trace computations offloaded onto AMD, Intel, and NVIDIA GPUs by using one of the following event specifiers:

- `-e gpu=nvidia` is used with CUDA and OpenMP on NVIDIA GPUs

- `-e gpu=amd` is used with HIP and OpenMP on AMD GPUs

- `-e gpu=level0` is used with Intel's Level Zero runtime for Data Parallel C++ and OpenMP

- `-e gpu=opencl` can be used on any of the GPU platforms.

Adding a `-t` to `hpcrun`'s command line when profiling GPU computations will trace them as well.

For more information about how to use PC sampling (NVIDIA GPUs only) or binary instrumentation (Intel GPUs) for instruction-level performance measurement of GPU kernels, see Chapter [8](#chpt:gpu).

### Recovering Program Structure

Typically, `hpcstruct` is launched without any options, with an argument that is a HPCToolkit *measurement directory*.
`hpcstruct` identifies the application as well as any shared libraries and GPU binaries it invokes.
It processes each of them and records information its program structure in the *measurements directory*.
Program structure for a binary includes information about its source files, procedures, inlined code, loop nests, and statements.

When applied to a measurements directory, `hpcstruct` analyzes multiple binaries concurrently by default.
It analyzes each small binary using a few threads and each large binary using more threads.

Although not usually necessary, one can apply `hpcstruct` to recover program structure information for a single CPU or GPU binary.
To recover static program structure for a single binary `b`, use the command:

> `hpcstruct b`

This command analyzes the binary and saves this information in a file named `b.hpcstruct`.

### Analyzing Measurements & Attributing Them to Source Code

To analyze HPCToolkit's measurements and attribute them to the application's source code, use `hpcprof`, typically invoked as follows:

> ```
> hpcprof hpctoolkit-app-measurements
> ```

This command will produce an HPCToolkit performance database with the name `hpctoolkit-app-database`.
If this database directory already exists, `hpcprof` will form a unique name by appending a random hexadecimal qualifier.

`hpcprof` performs this analysis in parallel using multithreading.
By default all available threads are used.
If this is not wanted (e.g. using sharing a single machine), the thread count can be specified with `-j <threads>`.

`hpcprof` usually completes this analysis in a matter of minutes.
For especially large experiments (applications using thousands of threads and/or GPU streams), the sibling `hpcprof-mpi` may produce results faster by exploiting additional compute nodes[^3].
Typically `hpcprof-mpi` is invoked as follows, using 8 ranks and compute nodes:

> ```
> <mpi-launcher> -n 8 hpcprof-mpi hpctoolkit-app-measurements
> ```

Note that additional options may be needed to grant `hpcprof-mpi` access to all threads on each node, check the documentation for your scheduler and MPI implementation for details.

If possible, `hpcprof` will copy the sources for the application and any libraries into the resulting database.
If the source code was moved since or was mounted at a different location than when the application was compiled, the resulting database may be missing some important source files.
In these cases, the `-R/--replace-path` option may be specified to provide substitute paths based on prefixes.
For example, if the application was compiled from source at `/home/joe/app/src/` but it is mounted at `/extern/homes/joe/app/src/` when running `hpcprof`, the source files can be made available by invoking `hpcprof` as follows:

> ```
> hpcprof -R `/home/joe/app/src/=/extern/homes/joe/app/src/' \
>   hpctoolkit-app-measurements
> ```

Note that on systems where MPI applications are restricted to a scratch file system, it is the users responsibility to copy any wanted source files and make them available to `hpcprof`.

### Presenting Performance Measurements for Interactive Analysis

To interactively view and analyze an HPCToolkit performance database, use `hpcviewer`.
`hpcviewer` may be launched from the command line or by double-clicking on its icon on MacOS or Windows.
The following is an example of launching from a command line:

> `hpcviewer hpctoolkit-app-database`

Additional help for `hpcviewer` can be found in a help pane available from `hpcviewer`'s *Help* menu.

### Effective Performance Analysis Techniques

To effectively analyze application performance, consider using one of the following strategies, which are described in more detail in Chapter [4](#chpt:effective-performance-analysis).

- A waste metric, which represents the difference between achieved performance and potential peak performance is a good way of understanding the potential for tuning the node performance of codes (Section [4.3](#sec:effective-performance-analysis:inefficiencies)).
  `hpcviewer` supports synthesis of derived metrics to aid analysis.
  Derived metrics are specified within `hpcviewer` using spreadsheet-like formula.
  See the `hpcviewer` help pane for details about how to specify derived metrics.

- Scalability bottlenecks in parallel codes can be pinpointed by differential analysis of two profiles with different degrees of parallelism (Section [4.4](#sec:effective-performance-analysis:scalability)).

## Additional Guidance

For additional information, consult the rest of this manual and other documentation:
First, we summarize the available documentation and command-line help:

Command-line help:
: Each of HPCToolkit's command-line tools can generate a help message summarizing the tool's usage, arguments and options.
  To generate this help message, invoke the tool with `-h` or `--help`.

Man pages:
: Man pages are available either via the Internet (<http://hpctoolkit.org/documentation.html>) or from a local HPCToolkit installation (`<hpctoolkit-installation>/share/man`).

Manuals:
: Manuals are available either via the Internet (<http://hpctoolkit.org/documentation.html>) or from a local HPCToolkit installation (`<hpctoolkit-installation>/share/doc/hpctoolkit/documentation.html`).

Articles and Papers:
: There are a number of articles and papers that describe various aspects of HPCToolkit's measurement, analysis, attribution and presentation technology.
  They can be found at <http://hpctoolkit.org/publications.html>.

[^3]: We recommend running `hpcprof-mpi` across 8-10 compute nodes. More than this may not improve or may degrade the overall speed of the analysis.
