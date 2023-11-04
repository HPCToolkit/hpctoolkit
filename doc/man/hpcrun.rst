======
hpcrun
======
--------------------------------------------------------------------------------------------------------------------------------------------------------------
profiling tool that collects call path profiles of program executions using statistical sampling of hardware counters, software counters, or timers.
--------------------------------------------------------------------------------------------------------------------------------------------------------------

:manual_section: 1
:manual_group: The HPCToolkit Performance Tools
:date: @DATE@
:version: @PROJECT_VERSION@
:author:
  Rice University's HPCToolkit Research Group:
  <`<https://hpctoolkit.org/>`_>
:contact: <`<hpctoolkit-forum@rice.edu>`_>
:copyright:
  Copyright © 2002-2023 Rice University.
  HPCToolkit is distributed under @LICENSE_RST@.

See |hpctoolkit(1)| for an overview of **HPCToolkit**.

SYNOPSIS
========

| ``hpcrun`` [*profiling_options*]... [``--``] *command* [*command_arguments*]...
|
| ``hpcrun`` [*info_options*]...

DESCRIPTION
===========

hpcrun profiles the execution of an arbitrary command *command* using statistical sampling.
hpcrun can profile an execution using multiple sample sources simultaneously, supports measurement of applications with multiple processes and/or multiple threads, and handles complex runtime behaviors including fork, exec, and/or dynamic loading of shared libraries.
hpcrun can be used in conjunction with program launchers such as mpiexec, SLURM's srun, LSF's jsrun, and Cray's aprun.

Note that hpcrun is unable to profile static executables.

To configure hpcrun's sampling sources, specify events and periods using
the -e/--event option.
For an event *e* and period *p*, after every *p* instances of *e*, a sample is generated that causes hpcrun to inspect the current calling context and augment its execution measurements of the monitored *command*.

If no sample source is specified, by default hpcrun profile using the timer ``CPUTIME`` on Linux at a frequency of 200 samples per second per thread.

When *command* terminates, a profile measurement database will be written to the directory:

.. parsed-literal::
  hpctoolkit-*command*-measurements[-*jobid*]

where *jobid* is a parallel job launcher id associated with the execution, if available.

hpcrun allows you to abort an execution and write the partial profiling data to disk by sending a signal such as SIGHUP or SIGINT (which is often bound to Control-c).
This can be extremely useful to collect data for long-running or misbehaving applications.

ARGUMENTS
=========

COMMAND
-------

*command*
   The command to profile.

*command_arguments*...
   Arguments to the command to profile.

OPTIONS: INFORMATIONAL
----------------------

-l, -L, --list-events  List available events. (N.B.: some may not be profilable)
-V, --version  Print version information.
-h, --help  Print help.

OPTIONS: PROFILING
------------------

``-ds``, ``--delay-sampling``
  Don't start sampling until the application enables sampling under program control.
  Use this option to measure specific intervals in an application's execution by bracketing code regions where measurement is desired with calls to ``hpctoolkit_sampling_start()`` and ``hpctoolkit_sampling_stop()``.
  Sampling may be started and stopped any number of times during an execution; measurements from all measurement intervals are aggregated.

--disable-gprof
  Override and disable gprof instrumentation.
  This option is only useful when using hpcrun to add HPCToolkit's measurement subsystem to a dynamically-linked application that has been compiled with ``-pg``.
  One can't measure performance with HPCToolkit when gprof instrumentation is active in the same execution.

-e <event[@howoften]>, --event <event[@howoften]>
  *event* may be an architecture-independent hardware or software event supported by Linux perf, a native hardware counter event, a hardware counter event supported by the PAPI library, or a Linux system timer (``CPUTIME`` and ``REALTIME``).
  This option may be given multiple times to profile several events at once.
  While events measured using the Linux perf monitoring infrastructure will be transparently multiplexed if necessary, for other sampling sources or on operating systems such as the Blue Gene Compute Node Kernel, there may be system-dependent limits on how many events can be profiled simultaneously and on which events may be combined for profiling.
  If the value for *howoften* is a number, it will be interpreted as a sample period.
  For Linux perf events, one may specify a sampling frequency for *howoften* by writing f before a number.
  For instance, to sample an event 100 times per second, specify *howoften* as ``@f100``.
  For Linux perf events, if no value for *howoften* is specified, hpcrun will monitor the event using frequency-based sampling at 300 samples/second.

  - For timer events CPUTIME and REALTIME, the units for a sample period are microseconds.

  - Timer events should not be mixed with hardware counter events.

  - See the "Sample sources" under **NOTES** for additional details.

-e <gpu=nvidia[,pc]>, --event <gpu=nvidia[,pc]>
  Collect comprehensive operation-level measurements for CUDA programs on NVIDIA GPUs, including timing of GPU kernel invocations, memory copies (implicit and explicit), driver and runtime activity, and overhead.
  If the optional argument ``pc`` is used, the GPU will collect instruction-level measurements of GPU kernels using Program Counter (PC) sampling in addition to the operation-level data.
  PC sampling attributes STALL reasons to individual GPU instructions.

-e <gpu=amd>, --event <gpu=amd>
  Collect comprehensive operation-level measurements for HIP programs on AMD GPUs, including timing of GPU kernel invocations, memory copies (implicit and explicit), driver and runtime activity, and overhead.

-e <gpu=opencl>, --event <gpu=opencl>
  Collect comprehensive operation-level measurements for OpenCL programs on AMD, Intel, or NVIDIA GPUs, including timing of GPU kernel invocations, memory copies (implicit and explicit), driver and runtime activity, and overhead.
  The opencl measurement mode may also be used to measure executions of DPC++ programs compiled to Intel's OpenCL backend.

-c howoften, --count howoften
  Only available for events managed by Linux perf.
  This option specifies a default value for how often to sample.
  The value for *howoften* may be a number that will be used as a default event period or an ``f`` followed by a number, e.g. ``f100``, to specify a default sampling frequency in samples/second.

  By default, hpcrun will allow attribution of hardware counter events to have arbitrary skid.
  Some processor architectures, e.g., ARM, don't support attribution with any higher level of precision.
  If a processor does not support the specified level of attribution precision for a hardware counter event, hpcrun may record 0 occurrences of the event without reporting an error.

-f *frac*, ``-fp`` *frac*, --process-fraction *frac*
  Measure only a fraction *frac* of the execution's processes.
  For each process, enable measurement of each thread with probability *frac*, a real number or a fraction (1/10) between 0 and 1.
  To minimize perturbations, when measurement for a process is disabled all threads in a process still receive sampling interrupts but they are ignored.

``-lm`` *size*, --low-memsize *size*
  Allocate an additional segment to store measurement data whenever free space in the current segment is less than the specified *size*.
  If not given, the default for *size* is 80K.

-m switch, --merge-threads switch
  Merge non-overlapped threads into one virtual thread.
  This option is to reduce the number of generated profile and trace files as each thread generates its own profile and trace data.
  The options are:

  0
    do not merge non-overlapped threads

  1
    merge non-overlapped threads (default)

``-ms`` *size*, ``--memsize`` *size*
  Use the specified *size* as segment size when allocating memory for measurement data.
  The specified value is rounded up to a multiple of the system page size.
  If not given, the default for *size* is 4M.

``-mp`` *prob*, ``--memleak-prob`` *prob*
  Monitor a subset of memory allocations performed by the application to detect leaks.
  An allocation is a call to one of malloc, calloc, realloc, etc and its matching call to free.
  At each allocation HPCToolkit generates a pseudo-random number in the range [0.0, 1.0) and monitors the allocation if the number is less than the value *prob* specified here.
  The value may be written as a a floating point number or as a fraction.
  If not given, the default for *prob* is 0.1.

-o outpath, --output outpath
  Directory to receive output data.
  If not given, the default directory is:

  .. parsed-literal::
    hpctoolkit-*command*-measurements[-*jobid*]

  .. CAUTION::
    If no *jobid* is available and no output option is given,
    profiles from multiple runs of the same *command* will be placed into
    the same output directory, which may lead to confusing or incorrect
    analysis results.

-r, --retain-recursion
  Do not collapse simple recursive call chains.
  Normally as hpcrun monitors an application that employs simple recursion, it collapses call chains of recursive calls to a single level.
  This design enables a user to see how the aggregate costs of recursion are associated with each recursive call yet saves space and time during post-mortem analysis by collapsing long chains of recursive calls.
  If this option is given, hpcrun will record all elements of a recursive call chain.

  .. note::
    When you use the RETCNT sample source this option is enabled automatically to gather accurate counts.

``-nu``, ``--no-unwind``
  Suppress unwinding of callstacks.
  Normally as hpcrun attributes performance metrics to full calling contexts.
  If this option is given, hpcrun collect only flat profiles, attributing metrics directly to functions without any information about the contexts in which they are called.

-t, --trace
  Generate a call path trace in addition to a call path profile.
  This option will enable tracing for CPUs if a time-based metric, such as ``CPUTIME``, ``REALTIME``, or ``cycles`` is used.
  This option will enable tracing for GPU operations if a ``-e gpu=*`` option is used to enable measurement of GPU activities.

``-tt``, ``--ttrace``
  Generate a call path trace that includes both sample and kernel launches on the CPU in addition to a call path profile.
  Since additional non-sample elements are added, any statistical properties of the CPU traces are disturbed.
  Also see ``--trace``.

OPTIONS: HPCTOOLKIT DEVELOPMENT
-------------------------------

These options are intended for use by the HPCToolkit team, but could be helpful to others interested in HPCToolkit's implementation.

-d, --debug
  After initialization, spin wait until you attach a debugger to one or more of the application's processes.
  After attaching you can set breakpoints or watchpoints in your application's code or in HPCToolkit's hpcrun code before beginning application execution.
  To continue after attaching, use the debugger to call ``hpcrun_continue()`` and then resume execution.

``-dd`` *flag*, ``--dynamic-debug`` *flag*
  Enable the flag *flag*, causing hpcrun to log debug messages guarded with that *flag* during execution.
  A list of dynamic debug flags can be found in HPCToolkit's source code in the file ``src/tool/hpcrun/messages/messages.flag-defns``.
  Note that not all flags are meaningful on all architectures.
  The special value ``ALL`` enables all debug flags.

  .. CAUTION::
    Turning on debug flags produces many log messages, often dramatically slowing the application and potentially distorting the measured profile.

``-md``, ``--monitor-debug``
  Enable debug tracing of libmonitor, the hpcrun subsystem which implements process/thread control.

--fnbounds-eager-shutdown
  Terminate the hpcfnbounds server when it goes idle.
  By default, it is kept alive during the entire run.

ENVIRONMENT VARIABLES
=====================

To function correctly, hpcrun must know the location of the HPCToolkit top-level installation directory so that it can access toolkit components located in its ``lib/`` and ``libexec/`` subdirectories.
Under most circumstances, hpcrun requires no special environment variable settings.

There are two situations, however, where hpcrun *must* consult the ``HPCTOOLKIT`` environment variable to determine the location of the top-level installation directory:

- On some systems, parallel job launchers (e.g., Cray's ``aprun``) *copy* the hpcrun script to a different location.
  For hpcrun to know the location of its top-level installation directory, you must set the ``HPCTOOLKIT`` environment variable to the top-level installation directory.

- If you launch hpcrun script via a file system link, you must set ``HPCTOOLKIT`` for the same reason.

LAUNCHING
=========

When sampling with native events, by default hpcrun will profile using perf events.
To force HPCToolkit to use PAPI (assuming it's available) instead of perf events, one must prefix the event with ``papi::`` as follows:

.. code:: bash

  hpcrun -e papi::CYCLES

For PAPI presets, there is no need to prefix the event with ``papi::``.
For instance it is sufficient to specify ``PAPI_TOT_CYC`` event without any prefix to profile using PAPI.

To sample an execution 100 times per second (frequency-based sampling) counting ``CYCLES`` and 100 times a second counting ``INSTRUCTIONS``:

.. code:: bash

  hpcrun -e CYCLES@f100 -e INSTRUCTIONS@f100 ...

To sample an execution every 1,000,000 cycles and every 1,000,000 instructions using period-based sampling:

.. code:: bash

  hpcrun -e CYCLES@1000000 -e INSTRUCTIONS@1000000 ...

By default, hpcrun will use frequency-based sampling with the rate 300 samples per second per event type.
Hence the following command will cause HPCToolkit to sample ``CYCLES`` at 300 samples per second and ``INSTRUCTIONS`` at 300 samples per second:

.. code:: bash

  hpcrun -e CYCLES -e INSTRUCTIONS ...

One can a different default rate using the ``-c`` option.
The command below will sample ``CYCLES`` at 200 samples per second and ``INSTRUCTIONS`` at 200 samples per second:

.. code:: bash

  hpcrun -c f200 -e CYCLES -e INSTRUCTIONS ...

EXAMPLES
========

Assume we wish to profile the application ``zoo``.
The following examples lists some useful events for different processor architectures.

.. code:: bash

  hpcrun -e CYCLES -e INSTRUCTIONS zoo

.. code:: bash

  hpcrun -e REALTIME@5000 zoo

.. code:: bash

  hpcrun -e DC_L2_REFILL@1300013 -e PAPI_L2_DCM@510011 \
    -e PAPI_STL_ICY@5300013 -e PAPI_TOT_CYC@13000019 \
    zoo

.. code:: bash

  hpcrun -e PAPI_L2_DCM@510011 -e PAPI_TLB_DM@510013 \
    -e PAPI_STL_ICY@5300013 -e PAPI_TOT_CYC@13000019 \
    zoo

NOTES
=====

Sample sources
--------------

hpcrun uses Linux perf_events (default on Linux platform) and optionally the PAPI library to provide access to hardware performance counter events.
It is important to note that on most out-of-order pipelined architectures, a hardware counter interrupt is not precisely attributed to the instruction that induced the counter to overflow.
The gap is commonly 50-70 instructions.
This means that one should not assume that aggregation at the source line level is fully precise.
(E.g., if a L1 D-cache miss is attributed to a statement that has been compiled to register-only operations, assume the miss is attributed to a nearby load.)
However, aggregation at the procedure and loop level is reliable.

Linux perf_events Interface
---------------------------

Linux perf_events provides a powerful interface that supports measurement of both application execution and kernel activity.
Using perf_events, one can measure both hardware and software events.
Using a processor's hardware performance monitoring unit (PMU), the perf_events interface can measure an execution using any hardware counter supported by the PMU.
Examples of hardware events include cycles, instructions completed, cache misses, and stall cycles.
Using instrumentation built in to the Linux kernel, the perf_events interface can measure software events.
Examples of software events include page faults, context switches, and CPU migrations.

HPCToolkit uses libpfm4 to translate from an event name string to an event code recognized by the kernel.
An event name is case insensitive and is defined as followed:

.. parsed-literal::

  [*pmu*::][*event_name*][:\ *unit_mask*]...[:\ *modifier*\[=\ *value*]]...

*pmu*
  Optional name of the PMU (group of events) to which the event belongs to.
  This is useful to disambiguate events in case events from difference sources have the same name.
  If no pmu is specified, the first match event is used.

*event_name*
  The name of the event.
  It must be the complete name, partial matches are not accepted.

*unit_mask*
  This designate an optional sub-events.
  Some events can be refined using sub-events.
  An event may have multiple unit masks and it is possible to combine them (for some events) by repeating ':\ *unit_mask*' pattern.

*modifier*\[=\ *value*]
  A modifier is an optional filter which modifies how the event counts.
  Modifiers have a type and a value specified after the equal sign.
  For boolean type modifiers, without specifying the value, the presence of the modifier is interpreted as meaning true.
  Events may support multiple modifiers, by repeating the ':\ *modifier*\[=\ *value*]' pattern.

*precise_ip*
  For some events, it is possible to control the amount of skid.
  Skid is a measure of how many instructions may execute between an event and the PC where the event is reported.
  Smaller skid enables more accurate attribution of events to instructions.
  Without a skid modifier, hpcrun allows arbitrary skid because some architectures don't support anything more precise.
  One may optionally specify one of the following as a skid modifier:

  :p
    a sample must have constant skid.

  :pp
    a sample is requested to have 0 skid.

  :ppp
    a sample must have 0 skid.

  :P
    autodetect the least skid possible.

  .. note::
   If the kernel or the hardware does not support the specified value of the skid, no error message will be reported but no samples will be delivered.

Some capabilities of HPCToolkit's perf_events Interface include:

Frequency-based sampling
  Rather than picking a sample period for a hardware counter, the Linux perf_events interface enables one to specify the desired sampling frequency and have the kernel automatically select and adjust the period to try to achieve the desired sampling frequency.
  To use frequency-based sampling, one can specify the sampling rate for an event as the desired number of samples per second by prefixing the rate with the letter ``f``.

Multiplexing
  Using multiplexing enables one to monitor more events in a single execution than the number of hardware counters a processor can support for each thread.
  The number of events that can be monitored in a single execution is only limited by the maximum number of concurrent events that the kernel will allow a user to multiplex using the perf_events interface.

  When more events are specified than can be monitored simultaneously using a thread's hardware counters, the kernel will employ multiplexing and divide the set of events to be monitored into groups, monitor only one group of events at a time, and cycle repeatedly through the groups as a program executes.

Thread blocking
  When a program executes, a thread may block waiting for the kernel to complete some operation on its behalf.
  Example operations include waiting for a read operation to complete or having the kernel service a page fault or zero-fill a page.
  On systems running Linux 4.3 or newer, one can use the perf_events sample source to monitor how much time a thread is blocked and where the blocking occurs.
  To measure the time a thread spends blocked, one can profile with ``BLOCKTIME`` event and another time-based event, such as ``CYCLES``.
  The ``BLOCKTIME`` event shouldn't have any frequency or period specified, whereas ``CYCLES`` should have a frequency or period specified.

PAPI Interface (optional)
-------------------------

The PAPI library supports a large collection of hardware counter events.
Some events have standard names across all platforms, e.g. ``PAPI_TOT_CYC``, the event that measures total cycles.
In addition to events whose names begin with the ``PAPI_`` prefix, platforms also provide access to a set of native events with names that are specific to the platform's processor.
A complete list of events supported by the PAPI library for your platform may be obtained by using the ``--list-events`` option.
Any event whose name begins with the ``PAPI_`` prefix that is listed as "Profilable" can be used as an event in a sampling source provided it does not conflict with another event.

The rules of thumb for selecting an appropriate set of events and their associated periods are complex.

Choosing sampling events
  Some PAPI events are not profilable because of PAPI implementation details.
  Also, PAPI's standard event list may not cover an architectural feature you are interested in.
  In such cases, it is necessary to resort to native events.
  In many cases, you will have to consult the architecture's manual to fully understand what the event means: there is no standard event list or naming scheme and events sometimes have unusual meanings.

Number of sampling events
  hpcrun does not multiplex hardware counters for events measured using PAPI.
  (Events measured using the Linux perf interface will be multiplexed automatically.)
  Without multiplexing, the number of events that you may use to profile a single execution is limited by your architecture's performance monitoring unit.
  Note that some architectures hard-wire one or more counters to a specific event (such as ``cycles``).

Choosing sampling periods
  The key requirement in choosing sampling periods is that you obtain enough samples to provide statistical significance.
  We usually recommend a sampling rate between 100s-1000s of samples per second.
  This usually only produces 1-5% execution time overhead.

  Choosing sampling rates depends on the architecture and sometimes the application.

  Choosing periods for cycle and instruction-related events are usually easy.
  Cycles directly relates to the clock speed.
  Instruction-related events relates to the issue rate and width.

  Choosing periods for other events seems harder because different applications uses resources differently.
  For example, some applications are memory intensive and others are not.
  However, if the goal is to identify rate-limiting factors of the architecture, then it is not necessary to consider the application.
  For example, if the goal is to determine whether L2 D-cache latency is a limiting factor, then it is only necessary to work backward from the architecture's specifications to determine what number of L2 D-cache misses per second would be problematic.

Architectural event conflicts
  With some performance monitoring units, certain events may not be concurrently used with other events.

System itimer (``WALLCLOCK``)
-----------------------------

On Linux systems, the kernel will not deliver itimer interrupts faster than the unit of a jiffy, which defaults to 4 milliseconds; see the itimer man page.
One can configure the kernel to use a value as small as 1 millisecond, but it is unlikely the kernel will actually deliver itimer signals at that rate when a period of 1000 microseconds is requested.

However, on Linux one can get quite close to the kernel Hz rate by setting the itimer interval to something less than the Hz rate.
For example, if the Hz rate is 1000 microseconds, one can use 500 microseconds (or just 1) and obtain about 999 interrupts per second.

PLATFORM-SPECIFIC NOTES
-----------------------

Cray Systems
------------

When using dynamically linked binaries on Cray systems, you should add the ``HPCTOOLKIT`` environment variable to your launch script.
Set ``HPCTOOLKIT`` to the top-level HPCToolkit install prefix (the directory containing the ``bin/``, ``lib/`` and ``libexec/`` subdirectories) and export it to the environment.
This is only needed for running dynamically linked binaries.
For example:

.. code:: bash

  #!/bin/sh
  #PBS -l mppwidth=#nodes
  #PBS -l walltime=00:30:00
  #PBS -V

  export HPCTOOLKIT=/path/to/hpctoolkit/install/directory

  # ...Rest of Script...

If ``HPCTOOLKIT`` is not set, you may see errors such as the following in
your job's error log.

::

   /var/spool/alps/103526/hpcrun: Unable to find HPCTOOLKIT root directory.
   Please set HPCTOOLKIT to the install prefix, either in this script, or in your environment, and try again.

The problem is that the Cray ALPS job launcher copies the hpcrun script to a directory somewhere below ``/var/spool/alps/`` and runs it from there.
By moving hpcrun to a different directory, this breaks hpcrun's default method for finding HPCToolkit's top-level installation directory.
The solution is to add ``HPCTOOLKIT`` to your environment so that hpcrun can find HPCToolkit's top-level installation directory.

MISCELLANEOUS
-------------

- hpcrun uses preloaded shared libraries to initiate profiling.
  For this reason, it cannot be used to profile setuid programs.

- hpcrun may not be able to profile programs that themselves use preloading.

SEE ALSO
========

|hpctoolkit(1)|

.. |hpctoolkit(1)| replace:: **hpctoolkit**\(1)
