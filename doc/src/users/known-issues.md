<!--
SPDX-FileCopyrightText: 2002-2023 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

# Known Issues

This section lists some known issues and potential workarounds.
Other known issues can be seen in the project's Gitlab issues pages:

- For HPCToolkit in general, see <https://gitlab.com/HPCToolkit/HPCToolkit/issues>

- For hpcviewer, see <https://gitlab.com/HPCToolkit/HPCViewer/issues>

## Inaccurate attribution of performance information can occur on Sapphire Rapids processors with HPCToolkit

Description:
: HPCToolkit depends upon the Dyninst binary analysis framework. Prior to Dyninst version 13.0, Dyninst stops its binary analysis of functions whenever it sees an a bit pattern that it doesn't recognize as a machine instruction. Unfortunately, Dyninst currently lacks support for decoding the AMX (advanced matrix extension) instructions supported by Sapphire Rapids. Prior to version 13.0, Dyninst will stop analysis of machine code in a function if it encounters an AMX instruction. This shortcoming in Dyninst can cause `hpcstruct` to fail to fully recover program structure for functions, leading to inaccurate attribution of program performance by HPCToolkit.

Workaround:
: Building HPCToolkit atop Dyninst 13.0 resolves the issue.

## We observed bad generated code for Dyninst using gcc 11.2.0 on Aurora

Description:
: On the Aurora supercomputer, we observed that hpcstruct crashed with a segmentation fault when it and its Dyninst dependence were compiled with gcc 11.2.0. Debugging showed that when a class in Dyninst called a function implemented by its base class, the wrong `this` pointer value was passed to the function, causing a segmentation fault.

Workaround:
: Recompiling HPCToolkit and Dyninst with gcc 12.2.0 eliminated the problem.

## When monitoring applications that use ROCm 6.0.0, using LD_AUDIT in `hpcrun` may cause it to fail to elide OpenMP runtime frames

Description:

: When an application provides a runtime that supports the OpenMP tools API known as OMPT, normally in the OpenMP runtime frames between user code on call stacks are elided. However, have observed that when using Glibc's `LD_AUDIT` as part of HPCToolkit's measurement infrastructure and using Rocm 6.0.0, an application's TLS storage may be reinitialized during HPCToolkit's initialization; this clears some important HPCToolkit state information from thread local variables. As a result, the primary thread is not recognized as an OpenMP thread, which is necessary to elide runtime frames.

  The root cause of the problem is a bug in Glibc's `LD_AUDIT`. This is believed to affect all versions of Glibc. However, we have only observed this problem when using ROCm 6.0.0.

Workaround:

: Use the `--disable-auditor` option to `hpcrun`.

## When using Intel GPUs, `hpcrun` may report that substantial time is spent in a partial call path consisting of only an unknown procedure

Description:
: Binary instrumentation on Intel GPUs uses Intel's GTPin. GTPin runs in its own private namespace. Asynchronous samples collected in response to Linux timer or hardware counter events may often occur when GTPin is executing. With GTPin in a private namespace, its code and symbols are invisible to `hpcrun`, which causes a degenerate unwind consisting of only an unknown procedure.

Workaround:
: Don't collect Linux timer or hardware counter events on the CPU when using binary instrumentation to collect instruction-level performance measurements of kernels executing on Intel GPUs.

## `hpcrun` reports partial call paths for code executed by a constructor prior to entering main

Description:
: At present, all samples of code executed by constructors are reported as a partial call paths even if they are full unwinds. This occurs because HPCToolkit wasn't designed to attribute code that executes in constructors.

Workaround:
: Don't be concerned by partial call paths that unwind through `__libc_start_main` and `__lib_csu_init`. The samples are fully attributed even though HPCToolkit does not recognize them as such.

Development Plan:
: A future version of HPCToolkit will recognize that these unwinds are indeed full call paths and attribute them as such.

## `hpcrun` may fail to measure a program execution on a CPU with hardware performance counters

Description:
: We observed a problem using Linux `perf_events` to measure CPU performance using hardware performance counters on an `x86_64` cluster at Sandia. An investigation determined that the cluster was running Sandia's LDMS (Lightweight Distributed Metric Service)---a low-overhead, low-latency framework for collecting, transferring, and storing metric data on a large distributed computer system. On this cluster, the LDMS daemon had been configured to use the `syspapi_sampler` (<https://github.com/ovis-hpc/ovis/blob/OVIS-4/ldms/src/sampler/syspapi/syspapi_sampler.c>), which uses the Linux `perf_events` subsystem to measure hardware counters at the node level. At present, the LDMS `syspapi_sampler`'s use of the Linux `perf_events` subsystem for data collection at the node level conflicts with native use of use the Linux `perf_events` subsystem by HPCToolkit for process-level measurement.[^17]

Workaround:
: Surprisingly, measurement using HPCToolkit's PAPI interface atop Linux `perf_events` works even though using HPCToolkit directly atop Linux `perf_events` yields no measurement data. For instance, rather than measuring `cycles` using Linux `perf_events` directly with `-e cycles`, one can measure cycles through HPCToolkit's PAPI measurement subsystem using `-e PAPI_TOT_CYC`. Of course, one can configure PAPI to measure other hardware events, such as graduated instructions and cache misses.

Development Plan:
: Identify why the use of the Linux `perf_events` subsystem by the LDMS `syspapi_sampler` conflicts with the use of the direct use of Linux `perf_events` HPCToolkit and the Linux `perf` tool but not with the use of Linux `perf_events` by PAPI.

## hpcrun may associate several profiles and traces with rank 0, thread 0

Description:
: On Cray systems, we have observed that `hpcrun` associates several profiles and traces with rank 0, thread 0. This results from the fact that the Cray PMI daemon gets forked from the application in a constructor and there is no exec. Initially, each process gets tagged with rank 0, thread 0 until the real rank and thread is determined later in the execution. That determination never happens for the PMI daemon.

Workaround:
: In our experience, the hpcrun files in the measurement for the daemon tagged with rank 0 thread 0 are very small. In experiments we ran, they were about 2K. You can remove these profiles and their matching trace files before processing a measurement database with `hpcprof`. The correspondence between a profile and trace can be determined because they only differ in their suffix (hpcrun or hpctrace).

## `hpcrun` sometimes enables writing of read-only data

If an application or shared library contains a `PT_GNU_RELRO` segment in its program header, the runtime loader `ld.so` will mark all data in that segment readonly
after relocations have been processed at runtime.
As described in Section [5.1.1.1](#hpcrun-audit) of the manual, on `x86_64` and Power architectures, `hpcrun` uses `LD_AUDIT` to monitor operations on dynamic libraries.
For `hpcrun` to properly resolve calls to functions in shared libraries, the Global Offset Table (GOT) must be writable. Sometimes, the GOT lies within the `PT_GNU_RELRO` segment, which may cause it to be marked readonly after relocations are processed.
If `hpcrun` is using `LD_AUDIT` to monitor shared library operations, it will enable write permissions on the `PT_GNU_RELRO` segment during execution. While this makes some data writable that should have read-only permissions, it should not affect the behavior of any program that does not attempt to overwrite data that should have been readonly in its address space.

## A confusing label for GPU theoretical occupancy

Affected architectures:

: NVIDIA GPUs

Description:

: When analyzing a GPU-accelerated application that employs NVIDIA GPUs, HPCToolkit estimates percent GPU theoretical occupancy as the ratio of active GPU threads divided by the maximum number of GPU threads available. In multi-threaded or multi-rank programs, HPCToolkit reports GPU theoretical occupancy with the label

  > Sum over rank/thread of exclusive 'GPU kernel: theoretical occupancy (FGP_ACT / FGP_MAX)'

  rather than its correct label

  > GPU kernel: theoretical occupancy (FGP_ACT / FGP_MAX)

  The metric is computed correctly by summing the fine-grain parallelism used in each kernel launch across all threads and ranks and dividing it by the sum of the maximum fine-grain parallelism available to each kernel launch across all threads and ranks, and presenting the value as a percent.

Explanation:

: This metric is unlike others computed by HPCToolkit. Rather than being computed by `hpcprof`, it is computed by having `hpcviewer` interpret a formula.

Workaround:

: Pay attention to the metric value, which is computed correctly and ignore its awkward label.

Development Plan:

: Add additional support to `hpcrun` and `hpcprof` to understand how derived metrics are computed and avoid spoiling their labels.

## Deadlock when using Darshan

Affected architectures:

: `x86_64` and ARM

Description:

: Darshan is a library for monitoring POSIX I/O. When using asynchronous sampling on the CPU to monitor a program that is being monitored with Darshan, your program may deadlock.

Explanation:

: Darshan hijacks calls to `open`.
  HPCToolkit uses the `libunwind` library.
  Under certain circumstances, `libunwind` uses `open` to inspect an application's executable or one of the shared libraries it uses to look for unwinding information recorded by the compiler.
  The following sequence of actions leads to a problem:

  1. A user application calls `malloc` and acquires a mutex lock on an allocator data structure.

  1. HPCToolkit's signal handler is invoked to record an asynchronous sample.

  1. `libunwind` is invoked to obtain the calling context for the sample.

  1. `libunwind` calls `open` to look for
     compiler-based unwind information.

  1. A Darshan wrapper for `open` executes in HPCToolkit's signal handler.

  1. The Darshan wrapper for `open` may try to allocate data to record statistics for the application's calls to `open`, deadlocking because a non-reentrant allocator lock is already held by this thread.

Workaround:

: Unload the Darshan module before compiling a statically-linked application or running a dynamically-linked application.

Development Plan:

: Ensure that `libunwind`'s calls to `open` are never intercepted by Darshan.

[^17]: We observed the same conflict between the LDMS `syspapi_sampler` and the Linux `perf` command-line tool. We expect that the `syspapi_sampler`
    conflicts with other process-level tools that use the Linux `perf_events` subsystem to measure events using hardware counters.
