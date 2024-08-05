<!--
SPDX-FileCopyrightText: 2002-2023 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

(sec:env)=

# Environment Variables

HPCToolkit's measurement subsystem decides what and how to measure
using information it obtains from environment variables.
This chapter describes all of the environment variables that control
HPCToolkit's measurement subsystem.

When using
HPCToolkit's `hpcrun` script to measure the performance of
dynamically-linked executables, `hpcrun` takes information passed
to it in command-line arguments and communicates it to HPCToolkit's
measurement subsystem by appropriately setting environment variables.
To measure statically-linked executables, one first adds HPCToolkit's
measurement subsystem to a binary as it is linked by using HPCToolkit's
`hpclink` script. Prior to launching a statically-linked binary that
includes HPCToolkit's measurement subsystem, a user
must manually set environment variables.

Section [13.1](#user-env) describes
environment variables of interest to users. Section [13.3](#system-env)
describes environment variables designed for use by HPCToolkit
developers. In some cases, HPCToolkit's developers will ask a user
to set some of the environment variables described in Section [13.3](#system-env) to generate a detailed error
report when problems arise.

(user-env)=

## Environment Variables for Users

`HPCRUN_EVENT_LIST`

: This environment variable is used provide a set of (event, period)
  pairs that will be used to configure HPCToolkit's measurement subsystem to perform
  asynchronous sampling. The HPCRUN_EVENT_LIST environment variable
  must be set otherwise HPCToolkit's measurement subsystem will terminate
  execution. If an application should run with sampling disabled,
  HPCRUN_EVENT_LIST should be set to NONE. Otherwise, HPCToolkit's
  measurement subsystem expects an event list of the form shown below.

  ```
  event1[@period1];...;eventN[@periodN]
  ```

  As denoted by the
  square brackets, periods are optional. The default period is 1
  million.

  Flags to add an event with `hpcrun`: `-e/--event event1@period1`

  Multiple events may be specified using multiple instances of `-e/--event` options.

`HPCRUN_TRACE`

: If this environment variable is set, HPCToolkit's measurement
  subsystem will collect a trace of sample events as part of a measurement
  database in addition to a profile. HPCToolkit's hpctraceviewer
  utility can be used to view the trace after the measurement database
  are processed with either HPCToolkit's hpcprof or hpcprofmpi
  utilities.

  Flags to enable tracing with `hpcrun`: `-t/--trace`

`HPCRUN_OUT_PATH`

: If this environment variable is set, HPCToolkit's measurement subsystem
  will use the value specified as the name of the directory where
  output data will be recorded. The default directory for a command
  *command* running under control of a job launcher with as job ID
  *jobid* is hpctoolkit-*command*-measurements\[-*jobid*\]. (If no
  job ID is available, the portion of the directory name in square
  brackets will be omitted. Warning: Without a *jobid* or an output
  option, multiple profiles of the same *command* will be placed
  in the same output directory.

  Flags to set output path with `hpcrun`: `-o/--output <directoryName>`

`HPCRUN_PROCESS_FRACTION`

: If this environment variable is set, HPCToolkit's measurement
  subsystem will measure only a fraction of an execution's processes.
  The value of HPCRUN_PROCESS_FRACTION may be written as a a floating
  point number or as a fraction. So, '0.10' and '1/10' are equivalent.
  If HPCRUN_PROCESS_FRACTION is set to a value with an unrecognized
  format, HPCToolkit's measurement subsystem will use the default
  probability of 0.1. For each process, HPCToolkit's measurement
  subsystem will generate a pseudo-random value in the range \[0.0, 1.0).
  If the generated random number is less than the value of
  HPCRUN_PROCESS_FRACTION, then HPCToolkit will collect performance
  measurements for that process.

  Flags to set process fraction with `hpcrun`: `-f/-fp/--process-fraction <frac>`

`HPCRUN_DELAY_SAMPLING`

: If this environment variable is set, HPCToolkit's measurement subsystem
  will initialize itself but not begin measurement using sampling
  until the program turns on sampling by calling
  `hpctoolkit_sampling_start()`. To measure only a part of a
  program, one can bracket that with `hpctoolkit_sampling_start()`
  and `hpctoolkit_sampling_stop()`. Sampling may be turned on
  and off multiple times during an execution, if desired.

  Flags to delay sampling with `hpcrun`: `-ds/--delay-sampling`

`HPCRUN_CONTROL_KNOBS`

: `hpcrun` has some settings, known as control knobs, that can be adjusted by a knowledgeable user to tune the operation of `hpcrun`'s measurement subsystem. Names and default values of the control knobs are shown in Table [13.1](#knob-names)

  ```{table} Control knob names and default values.
  ---
  name: knob-names
  ---
  | Name                                |   Default Value | Description |
  | :---------------------------------- | ------: | :---------- |
  | `MAX_COMPLETION_CALLBACK_THREADS`   |    1000 | See Note 1. |
  | `STREAMS_PER_TRACING_THREAD`        |       4 | See Note 2. |
  | `HPCRUN_CUDA_DEVICE_BUFFER_SIZE`    | 8388608 | See Note 3. |
  | `HPCRUN_CUDA_DEVICE_SEMAPHORE_SIZE` |   65536 | See Note 4. |
  ```

  **Note 1**: OpenCL may execute callbacks on helper threads created by the OpenCL runtime. This knob specifies the maximum number of helper threads that can be handled by `hpcrun`'s OpenCL tracing implementation.

  **Note 2**: GPU stream traces are recorded by tracing threads created by `hpcrun`. Reducing the number of streams per `hpcrun` tracing thread may make monitoring faster, though it will use more resources.

  **Note 3**: Value used as `CUPTI_ACTIVITY_ATTR_DEVICE_BUFFER_SIZE`. See <https://docs.nvidia.com/cuda/cupti/group__CUPTI__ACTIVITY__API.html>.

  **Note 4**: Value used as `CUPTI_ACTIVITY_ATTR_PROFILING_SEMAPHORE_POOL_SIZE`. See <https://docs.nvidia.com/cuda/cupti/group__CUPTI__ACTIVITY__API.html>

  Flags to set a control knob for `hpcrun`: `-ck/--control-knob` name=setting.

`HPCRUN_MEMLEAK_PROB`

: If this environment variable is set, HPCToolkit's measurement
  subsystem will measure only a fraction of an execution's memory
  allocations, e.g., calls to `malloc`, `calloc`, `realloc`,
  `posix_memalign`, `memalign`, and valloc. All allocations
  monitored will have their corresponding calls to free monitored as
  well. The value of HPCRUN_MEMLEAK_PROB may be written as a a
  floating point number or as a fraction. So, '0.10' and '1/10' are
  equivalent. If HPCRUN_MEMLEAK_PROB is set to a value with an
  unrecognized format, HPCToolkit's measurement subsystem will use the
  default probability of 0.1. For each memory allocation, HPCToolkit's
  measurement subsystem will generate a pseudo-random value in the range
  \[0.0, 1.0). If the generated random number is less than the value
  of HPCRUN_MEMLEAK_PROB, then HPCToolkit will monitor that
  allocation.

  Flags to set process fraction with `hpcrun`: `-mp/--memleak-prob <prob>`

`HPCRUN_RETAIN_RECURSION`

: Unless this environment variable is set, by default HPCToolkit's
  measurement subsystem will summarize call chains from recursive calls
  at a depth of two. Typically, application developers have no need
  to see performance attribution at all recursion depths when an
  application calls recursive procedures such as quicksort. Setting
  this environment variable may dramatically increase the size of
  calling context trees for applications that employ bushy subtrees
  of recursive calls.

  Flags to retain recursion with `hpcrun`: `-r/--retain-recursion`

`HPCRUN_MEMSIZE`

: If this environment variable is set, HPCToolkit's measurement subsystem
  will allocate memory for measurement data in segments using the
  value specified for HPCRUN_MEMSIZE (rounded up to the nearest
  enclosing multiple of system page size) as the segment size. The
  default segment size is 4M.

  Flags to set memsize with `hpcrun`: `-ms/--memsize <bytes>`

`HPCRUN_LOW_MEMSIZE`

: If this environment variable is set, HPCToolkit's measurement subsystem
  will allocate another segment of measurement data when the amount
  of free space available in the current segment is less than the
  value specified by HPCRUN_LOW_MEMSIZE. The default for low memory
  size is 80K.

  Flags to set low memsize with `hpcrun`: `-lm/--low-memsize <bytes>`

`HPCTOOLKIT_HPCSTRUCT_CACHE`

: If this environment variable contains the name of a Linux directory
  that is readable and writable to you, `hpcstruct` will cache any
  program structure files it computes in this directory. When invoked
  to analyze a binary, `hpcstruct` will check if program structure
  information for the binary exists in the cache. If so, `hpcstruct`
  will return the cached copy. If not, `hpcstruct` will compute
  program structure information for the binary and record it in the
  cache.

## Environment Variables that May Avoid a Crash

`HPCRUN_AUDIT_FAKE_AUDITOR`

: By default, `hpcrun` will use `libc`'s `LD_AUDIT` feature to monitor dynamic library operations.
  For cases where using `LD_AUDIT` is problematic (e.g. with applications or libraries that require the use of `dlmopen`) ,
  `hpcrun` supports an alternative *fake auditor* that monitors shared library operations by wrapping `dlopen` and `dlclose` instead. This variable will be set to 1 if a *fake auditor* is used.
  If `LD_AUDIT` is not causing your program to crash, we don't recommend using the fake auditor as it may cause your application or shared libraries it loads to ignore any `RUNPATH` set in their binaries.

  Flag to select the fake auditor with `hpcrun`: `--disable-auditor`.

`HPCRUN_AUDIT_DISABLE_PLT_CALL_OPT`

: By default, `hpcrun` will use `libc`'s `LD_AUDIT` feature to monitor dynamic library operations. The `LD_AUDIT` facility has the unfortunate behavior of intercepting each call to a shared library. Each call to a shared library is dispatched through the *Procedure Linkage Table* (PLT). We have observed that allowing the `LD_AUDIT` facility to intercept each call to a shared library is costly: on `x86_64` we measured a slowdown of 68x for a call to an empty shared library routine.

  To avoid this overhead, `hpcrun` sidesteps `LD_AUDIT`'s monitoring of a load module's calls to a shared library routine by allowing the address of the routine to be cached in the load module's Global Offset Table (GOT). The mechanism for this optimization is complex. If you suspect that this optimization is causing your program to crash, this optimization can be disabled. If your program is not crashing, don't even consider adjusting this!

  Flag to disable optimization of PLT calls when using `LD_AUDIT` to monitor shared library operations with `hpcrun`: `--disable-auditor-got-rewriting`.

(system-env)=

## Environment Variables for Developers

`HPCRUN_WAIT`

: If this environment variable is set, HPCToolkit's measurement subsystem
  will spin wait for a user to attach a debugger. After attaching a
  debugger, a user can set breakpoints or watchpoints in the user
  program or HPCToolkit's measurement subsystem before continuing
  execution. To continue after attaching a debugger, use the debugger
  to set the program variable DEBUGGER_WAIT=0 and then continue.
  Note: Setting HPCRUN_WAIT can only be cleared by a debugger
  if HPCToolkit has been built with debugging symbols.
  Building HPCToolkit with debugging symbols requires
  configuring HPCToolkit with --enable-develop.

`HPCRUN_DEBUG_FLAGS`

: HPCToolkit supports a multitude of debugging flags that enable a
  developer to log information about HPCToolkit's measurement subsystem
  as it records sample events. If HPCRUN_DEBUG_FLAGS is set, this
  environment variable is expected to contain a list of tokens separated
  by a space, comma, or semicolon. If a token is the name of a debugging
  flag, the flag will be enabled, it will cause HPCToolkit's measurement
  subsystem to log messages guarded with that flag as an application
  executes. The complete list of dynamic debugging flags can be found
  in HPCToolkit's source code in the file
  src/tool/hpcrun/messages/messages.flag-defns. A special flag value "ALL" enables all flags.
  Note: not all debugging flags are
  meaningful on all architectures.

  Caution: turning on debugging flags
  will typically result in voluminous log messages, which will typically will
  dramatically slow measurement of the execution under study.

  Flags to set debug flags with `hpcrun`: `-dd/--dynamic-debug <flag>`

`HPCRUN_ABORT_TIMEOUT`

: If an execution hangs when profiled with HPCToolkit's measurement
  subsystem, the environment variable HPCRUN_ABORT_TIMEOUT can be
  used to specify the number of seconds that an application should
  be allowed to execute. After executing for the number of seconds
  specified in HPCRUN_ABORT_TIMEOUT, HPCToolkit's measurement
  subsystem will forcibly terminate the execution and record a core
  dump (assuming that core dumps are enabled) to aid in debugging.

  Caution: for a large-scale parallel execution, this might cause a
  core dump for each process, depending upon the settings for your
  system. Be careful!
