<!--
SPDX-FileCopyrightText: 2002-2023 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

# HPCToolkit Overview

```{figure-md} fig:hpctoolkit-overview:a
![](hpctoolkit-gpu-workflow.png)

Overview of HPCToolkit's tool work flow.
```

HPCToolkit's work flow is organized around four principal capabilities, as shown in Figure [2.1](#fig:hpctoolkit-overview:a):

1. *measurement* of context-sensitive performance metrics using call-stack unwinding
   while an application executes;

1. *binary analysis* to recover program structure from the application binary and the shared libraries
   and GPU binaries used in the run;

1. *attribution* of performance metrics by correlating dynamic performance metrics with static program structure; and

1. *presentation* of performance metrics and associated source code.

To use HPCToolkit to measure and analyze an application's performance, one first compiles and links the application for a production run, using *full* optimization and including debugging symbols.
[^1]
Second, one launches an application with HPCToolkit's measurement tool, `hpcrun`, which uses statistical sampling to collect a performance profile.
Third, one invokes `hpcstruct`, HPCToolkit's tool for analyzing an application binary and any shared objects and GPU binaries
it used in the data collection run, as stored in the measurements directory. It recovers
information about source files, procedures, loops, and inlined code.
Fourth, one uses `hpcprof` to combine information about an application's structure with dynamic performance measurements to produce a performance database.
Finally, one explores a performance database with HPCToolkit's `hpcviewer` and/or Trace view graphical presentation tools.

The rest of this chapter briefly discusses unique aspects of HPCToolkit's measurement, analysis and presentation capabilities.

## Asynchronous Sampling and Call Path Profiling

Without accurate measurement, performance analysis results may be of questionable value.
As a result, a principal focus of work on HPCToolkit has been the design and implementation of techniques to provide accurate fine-grain measurements of production applications running at scale.
For tools to be useful on production applications on large-scale parallel systems, large measurement overhead is unacceptable.
For measurements to be accurate, performance tools must avoid introducing measurement error.
Both source-level and binary instrumentation can distort application performance through a variety of mechanisms (Mytkowicz et al. 2009).
Frequent calls to small instrumented procedures can lead to considerable measurement overhead.
Furthermore, source-level instrumentation can distort application performance by interfering with inlining and template optimization.
To avoid these effects, many instrumentation-based tools intentionally refrain from instrumenting certain procedures.
Ironically, the more this approach reduces overhead, the more it introduces *blind spots*, i.e., intervals of unmonitored execution.
For example, a common selective instrumentation technique is to ignore small frequently executed procedures --- but these may be just the thread synchronization library routines that are critical.
Sometimes, a tool unintentionally introduces a blind spot.
A typical example is that source code instrumentation suffers from blind spots when source code is unavailable, a common condition for math and communication libraries.

To avoid these problems, HPCToolkit eschews instrumentation and favors the use of *asynchronous sampling* to measure and attribute performance metrics.
During a program execution, sample events are triggered by periodic interrupts induced by an interval timer or overflow of hardware performance counters.
One can sample metrics that reflect work (e.g., instructions, floating-point operations), consumption of resources (e.g., cycles, bandwidth consumed in the memory hierarchy by data transfers in response to cache misses), or inefficiency (e.g., stall cycles).
For reasonable sampling frequencies, the overhead and distortion introduced by sampling-based measurement is typically much lower than that introduced by instrumentation (Froyd, Mellor-Crummey, and Fowler 2005).

For all but the most trivially structured programs, it is important to associate the costs incurred by each procedure with the contexts in which the procedure is called.
Knowing the context in which each cost is incurred is essential for understanding why the code performs as it does.
This is particularly important for code based on application frameworks and libraries.
For instance, costs incurred for calls to communication primitives (e.g., `MPI_Wait`) or code that results from instantiating C++ templates for data structures can vary widely depending how they are used in a particular context.
Because there are often layered implementations within applications and libraries, it is insufficient either to insert instrumentation at any one level or to distinguish costs based only upon the immediate caller.
For this reason, HPCToolkit uses call path profiling to attribute costs to the full calling contexts in which they are incurred.

HPCToolkit's `hpcrun` call path profiler uses call stack unwinding to attribute execution costs of optimized executables to the full calling context in which they occur.
Unlike other tools, to support asynchronous call stack unwinding during execution of optimized code, `hpcrun` uses on-line binary analysis to locate procedure bounds and compute an unwind recipe for each code range within each procedure (N. R. Tallent, Mellor-Crummey, and Fagan 2009).
These analyses enable `hpcrun` to unwind call stacks for optimized code with little or no information other than an application's machine code.

The output of a run with `hpcrun` is a *measurements directory* containing the data, and the information necessary
to recover the names of all shared libraries and GPU binaries.

## Recovering Static Program Structure

To enable effective analysis, call path profiles for executions of optimized programs must be correlated
with important source code abstractions.
Since measurements refer only to instruction addresses within the running application,
it is necessary to map measurements back to the program source.
The mappings include those of the application and any shared libraries referenced during the
run, as well as those for any GPU binaries executed on GPUs during the run.
To associate measurement data with the static structure of fully-optimized executables,
we need a mapping between object code and its associated source code structure.[^2]
HPCToolkit constructs this mapping using binary analysis; we call this process
*recovering program structure* (N. R. Tallent, Mellor-Crummey, and Fagan 2009).

HPCToolkit focuses its efforts on recovering source files, procedures, inlined functions and templates, as well as
loop nests as the most important elements of source code structure.
To recover program structure, HPCToolkit's `hpcstruct` utility parses a binary's machine instructions,
reconstructs a control flow graph, combines line map and DWARF information about inlining with interval
analysis on the control flow graph in a way that enables it to relate machine code after optimization
back to the original source.

One important benefit accrues from this approach.
HPCToolkit can expose the structure of and assign metrics to the code is actually executed, *even if source code is unavailable*.
For example, `hpcstruct`'s program structure naturally reveals transformations such as loop fusion and scalarization
loops that arise from compilation of Fortran 90 array notation.
Similarly, it exposes calls to compiler support routines and wait loops in communication libraries of which one would otherwise be unaware.

## Reducing Performance Measurements

HPCToolkit combines (post-mortem) the recovered static program structure with dynamic call paths to expose inlined frames and loop nests.
This enables us to attribute the performance of samples in their full static and dynamic context and correlate it with source code.

The data reduction is done by HPCToolkit's `hpcprof` utility, invoked on the *measurements directory* recorded by `hpcrun` and augmented with program structure information by `hpcstruct`.
From the measurements and structure, `hpcprof` generates a *database directory* containing performance data presentable by `hpcviewer`.

In most cases `hpcprof` is able to complete the reduction in a matter of minutes, however for especially large experiments (more than about 100,000 threads or GPU streams (Anderson, Liu, and Mellor-Crummey 2022)) its multi-node sibling `hpcprof-mpi` may be substantially faster.
`hpcprof-mpi` is an MPI application identical to `hpcprof`, except that it additionally can exploit multiple compute nodes during the reduction.
In our experience, exploiting 8-10 compute nodes via `hpcprof-mpi` can be as much as 5x faster than `hpcprof` for sufficiently large experiments.

## Presenting Performance Measurements

To enable an analyst to rapidly pinpoint and quantify performance bottlenecks, tools must present the performance measurements in a way that engages the analyst, focuses attention on what is important, and automates common analysis subtasks to reduce the mental effort and frustration of sifting through a sea of measurement details.

To enable rapid analysis of an execution's performance bottlenecks, we have carefully designed the `hpcviewer`

- a code-centric presentation tool (Adhianto, Mellor-Crummey, and Tallent 2010). It also includes a time-centric tab
  (N. R. Tallent et al. 2011).

`hpcviewer` combines a relatively small set of complementary presentation techniques that, taken together, rapidly focus an analyst's attention on performance bottlenecks rather than on unimportant information.
To facilitate the goal of rapidly focusing an analyst's attention on performance bottlenecks `hpcviewer`
extends several existing presentation techniques.
In particular, `hpcviewer` (1) synthesizes and presents three complementary views of calling-context-sensitive metrics;
(2) treats a procedure's static structure as first-class information with respect to both performance metrics
and constructing views; (3) enables a large variety of user-defined metrics to describe performance inefficiency;
and (4) automatically expands hot paths based on arbitrary performance metrics --- through calling contexts and static structure --- to rapidly highlight important performance data.

The trace tab enables an application developer to visualize how a parallel execution unfolds over time.
This view facilitates identification of important inefficiencies such as serialization and load imbalance, among others.

[^1]: For the most detailed attribution of application performance data using HPCToolkit, one should ensure that the compiler includes line map information in the object code it generates. While HPCToolkit does not need this information to function, it can be helpful to users trying to interpret the results. Since compilers can usually provide line map information for fully optimized code, this requirement need not require a special build process. For instance, with the Intel compiler we recommend using `-g -debug inline_debug_info`.

[^2]: This object to source code mapping should be contrasted with the binary's line map, which
    (if present) is typically fundamentally line based.
