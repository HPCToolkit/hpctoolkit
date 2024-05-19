<!--
SPDX-FileCopyrightText: 2002-2023 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

(chpt:statically-linked-apps)=

# Monitoring Statically Linked Applications with `hpclink`

On modern Linux systems, dynamically linked executables are the default.
With dynamically linked executables, HPCToolkit's `hpcrun` script uses library preloading to inject HPCToolkit's monitoring code into an application's address space.
However, in some cases, statically-linked executables are necessary or desirable.

- One might prefer a statically linked executable because they are generally faster if the executable spends a significant amount of time calling functions in libraries.

- On Cray supercomputers, statically-linked executables are often the default.

For statically linked executables, preloading HPCToolkit's monitoring code into an application's address space at program launch is not an option.
Instead, monitoring code must be added at link time; HPCToolkit's `hpclink` script is used for this purpose.

## Linking with `hpclink`

Adding HPCToolkit's monitoring code into a statically linked application is easy.
This does not require any source-code modifications, but it does involve a small change to your build procedure.
You continue to compile all of your object (`.o`) files exactly as before, but you will need to modify your final link step to use `hpclink` to add HPCToolkit's monitoring code to your executable.

In your build scripts, locate the last step in the build, namely, the command that produces the final statically linked binary.
Edit that command line to add the `hpclink` command at the front.

For example, suppose that the name of your application binary is `app` and the last step in
your `Makefile` links various object files and libraries as
follows into a statically linked executable:

> `mpicc -o app -static file.o ... -l<lib> ...`

To build a version of your executable with HPCToolkit's monitoring code linked in, you would use the following command line:

> `hpclink mpicc -o app -static file.o ... -l<lib> ...`

In practice, you may want to edit your `Makefile` to always build two versions of your program, perhaps naming them `app` and `app.hpc`.

### Using `hpclink` when `gprof` instrumentation is present

When an application has been compiled with the compiler flag `-pg`,
the compiler adds instrumentation to collect performance measurement data for
the `gprof` profiler. Measuring application performance with
HPCToolkit's measurement subsystem and `gprof` instrumentation
active in the same execution may cause the execution
to abort. One can detect the presence of `gprof` instrumentation in an
application by the presence of `__monstartup` and `_mcleanup` symbols
in a executable.
One can disable `gprof` instrumentation when measuring the performance of
a statically-linked application by using the `--disable-gprof`
argument to `hpclink`.

## Running a Statically Linked Binary

For dynamically linked executables, the `hpcrun` script sets environment variables to pass information to the HPCToolkit monitoring library.
On standard Linux systems, statically linked `hpclink`-ed executables can still be launched with `hpcrun`.

You many encounter a situation where the `hpcrun` script cannot be used with an application launcher.
In such cases, you will need to use the `HPCRUN_EVENT_LIST` environment variable to pass a list of events to HPCToolkit's monitoring code, which was linked into your executable using `hpclink`.
Typically, you would set `HPCRUN_EVENT_LIST` in your launch script.

The `HPCRUN_EVENT_LIST` environment variable should be set to a space-separated list of `EVENT@COUNT` pairs.
For example, in a PBS script for a Cray system, you might write the following in Bourne shell or bash syntax:

> ```
> #!/bin/sh
> #PBS -l size=64
> #PBS -l walltime=01:00:00
> cd $PBS_O_WORKDIR
> export HPCRUN_EVENT_LIST="CYCLES@f200 PERF_COUNT_HW_CACHE_MISSES@f200"
> aprun -n 64 ./app arg ...
> ```

To collect sample traces of an execution of a statically linked binary (for visualization with Trace view), one needs to set the environment variable `HPCRUN_TRACE=1` in the execution environment.

## Troubleshooting

With some compilers you need to disable interprocedural optimization to use `hpclink`.
To instrument your statically linked executable at link time, `hpclink` uses the `ld` option `--wrap` (see the ld(1) man page) to interpose monitoring code between your application and various process, thread, and signal control operations, e.g., `fork`, `pthread_create`, and `sigprocmask` to name a few.
For some compilers, e.g., IBM's XL compilers, interprocedural optimization interferes with the `--wrap` option and prevents `hpclink` from working properly.
If this is the case, `hpclink` will emit error messages and fail.
If you want to use `hpclink` with such compilers, sadly, you must turn off interprocedural optimization.

Note that interprocedural optimization may not be explicitly enabled during your compiles; it might be implicitly enabled when using a compiler optimization option such as `-fast`.
In cases such as this, you can often specify `-fast` along with an option such as `-no-ipa`; this option combination will provide the benefit of all of `-fast`'s optimizations *except* interprocedural optimization.
