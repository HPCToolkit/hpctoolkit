.. SPDX-License-Identifier: CC-BY-4.0
.. Copyright information is in the :copyright: field below

=========
hpcstruct
=========
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
recovers the static program structure of CPU or GPU binaries, as cited in a recorded measurements directory, or in a single CPU or GPU binary. A binary's static structure includes information about source files, procedures, inlined functions, loops, and source lines.
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

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

``hpcstruct`` [*options*]... *measurement_directory*

``hpcstruct`` [*options*]... *binary*

DESCRIPTION
===========

hpcstruct generates a program structure file for each CPU and GPU binary referenced by a directory containing HPCToolkit performance measurements.
hpcstruct is usually applied to an application's HPCToolkit *measurement_directory*, directing it to analyze all CPU and GPU binaries measured during an execution.
Although not normally needed, one can apply hpcstruct to recover program structure for an individual CPU or GPU binary.

Program structure is a mapping from addresses of machine instructions in a binary to source code contexts; this mapping is used to attribute measured performance metrics back to source code.
A strength of hpcstruct is its ability to attribute metrics to inlined functions and loops; such mappings are especially useful for understanding the performance of programs generated using template-based programming models.
When hpcprof is invoked on a *measurement_directory* that contains program structure files, they will be used attribute performance measurements.

hpcstruct is designed to cache its results so that processing multiple measurement directories can copy previously generated structure files from the cache rather than regenerating the information.
A cache may be specified either on the command line (see the ``-C`` option, below) or by setting the ``HPCTOOLKIT_HPCSTRUCT_CACHE`` environment variable.
If a cache is specified, hpcstruct emits a message for each binary processed saying one of "Added to cache", "Replaced in cache", "Copied from cache", or "Copied from cache +".
The latter indicates that although the structure file was found in the cache, it came from an identical binary with a different full path, so it was edited to reflect the actual path.
"Replaced in cache" indicates that a previous version at that path was replaced.
If the ``--nocache`` option is set, hpcstruct will not use the cache, even if the environment variable is set, and the message will say "(Cache disabled by user)".
If no cache is specified, the message will say "Cache not specified", and, unless the ``--nocache`` option was set, an ADVICE message urging use of the cache will be written.
Users are strongly urged to use a cache.

When an application execution is measured using hpcrun, HPCToolkit records the name of the application binary, and the names of any shared-libraries and GPU binaries it used.
As a GPU-accelerated application is measured, HPCToolkit also records the contents of any GPU binaries it loads in the application's measurement directory.

When analyzing a *measurement_directory*, hpcstruct writes its results into a subdirectory of the directory.
It analyzes the application and all the shared libraries and GPU binaries used during the run.
When rerun against a *measurement_directory*, hpcstruct will reprocess only those GPU binaries that were previously processed with a different ``--gpucfg`` setting.

To accelerate analysis of a measurement directory, which contains references to an application as well as any shared libraries and/or GPU binaries it uses, hpcstruct employs multiple threads by default.
A pool of threads equal to half of the threads in the CPU set for the process is used.
Binaries larger than a certain threshold (see the ``--psize`` option and its default) are analyzed using more OpenMP threads than those smaller than the threshold.
Multiple binaries are processed concurrently.
hpcstruct will describe the actual parallelization and concurrency used when the run starts.

When analyzing a single CPU or GPU binary */path/to/b*, hpcstruct writes its results to the file *b*\ ``.hpcstruct`` in the current directory.
When rerun against a single binary, it will reprocess that binary and rewrite its structure file.

hpcstruct is designed for analysis of optimized binaries created from C, C++, Fortran, CUDA, HIP, and DPC++ source code.
Because hpcstruct's algorithms exploit the line map and debug information recorded in an application binary during compilation, for best results, we recommend that binaries be compiled with standard debug information or at a minimum, line map information.
Typically, this is accomplished by passing a ``-g`` option to each compiler along with any optimization flags.
See the HPCToolkit manual for more information.

ARGUMENTS
=========

*measurement_directory*
  A measurement directory of an application, either GPU-accelerated or not.
  Applying hpcstruct to a measurement directory analyzes the application as well as all shared libraries and GPU binaries measured during the data-collection run.

*binary*
  File containing an executable, a dynamically-linked shared library, or a GPU binary recorded by HPCToolkit as a program executes.
  Note that hpcstruct does not recover program structure for libraries that *binary* depends on.
  To recover that structure, run hpcstruct on each dynamically-linked shared library or relink your program with static versions of the libraries.
  Invoking hpcstruct on a binary is normally not used.

Default values for an option's optional arguments are shown in {}.

OPTIONS: INFORMATIONAL
----------------------

-V, --version  Print version information.

-h, --help  Print help message.

-v num, --verbose num  Generate progress messages to stderr, at verbosity level *num*. {1}

OPTIONS: USING THE CACHE
------------------------

-c dir, --cache dir  Specify the directory to be used for the structure cache.

--nocache  Specify the the structure cache is not to be used, even if one is specified by the ``HPCTOOLKIT_HPCSTRUCT_CACHE`` environment variable.

OPTIONS: OVERRIDE PARALLEL DEFAULTS
-----------------------------------

-j num, --jobs num
  Specify the number of threads to be used.
  *num* OpenMP threads will be used to analyze any large binaries.
  A pool of *num* threads will be used to analyze small binaries.

--psize psize
  hpcstruct will consider any binary of at least *psize* bytes as large.
  hpcstruct will use more OpenMP threads to analyze large binaries than it uses to analyze small binaries.
  {``100000000``}

-s v, --stack v
  Set the stack size for OpenMP worker threads to *v*.
  *v* is a positive number optionally followed by a suffix: B (bytes), K (kilobytes), M (megabytes), or G (gigabytes).
  Without a suffix, *v* will be interpreted as kilobytes.
  One can also control the stack size by setting the ``OMP_STACKSIZE`` environment variable.
  The ``-s`` option takes precedence, followed by ``OMP_STACKSIZE``.
  {``32M``}

OPTIONS: OVERRIDE STRUCTURE RECOVERY DEFAULTS
---------------------------------------------

--cpu bool
  Analyze CPU binaries references in a measurements directory.
  *bool* is either ``yes`` or ``no``.
  {``yes``}

--gpu bool
  Analyze GPU binaries references in a measurements directory.
  *bool* is either ``yes`` or ``no``.
  {``yes``}

--gpucfg bool
  Compute loop nesting structure for GPU machine code.
  *bool* is either ``yes`` or ``no``.
  {``no``}

OPTIONS TO CONTROL OUTPUT:
--------------------------

-o filename, --output filename
  Write the output to to *filename*.
  This option is only applicable when invoking hpcstruct on a single binary.

OPTIONS FOR DEVELOPERS:
-----------------------

--pretty-print  Add indenting for more readable XML output.
--jobs-struct num  Use *num* threads for the program structure analysis phase of hpcstruct.
--jobs-parse num  Use *num* threads for the parse phase of hpcstruct.
--jobs-symtab num  Use *num* threads for the symbol table analysis phase of hpcstruct.

--show-gaps
  Developer option to write a text file describing all the "gaps" found by hpcstruct, i.e. address regions not identified as belonging to a code or data segment by the ParseAPI parser used to analyze application executables.
  The file is named *outfile*\ ``.gaps``, which by default is *appname*\ ``.hpcstruct.gaps``.

--time  Display the time and space usage per phase in hpcstruct.

OPTIONS FOR INTERNAL USE ONLY
-----------------------------

-M dir
  Indicates that hpcstruct was invoked by a script used to process measurement directory *dir*.
  This information is used to control messages.

EXAMPLES
========

1
---

Assume we have used HPCToolkit to collect performance measurements for the (optimized) CPU binary ``sweep3d`` and that performance measurement data for the application is in the measurement directory ``hpctoolkit-sweep3d-measurements``.
Assume that ``sweep3d`` was compiled with debugging information using the ``-g`` compiler flag in addition to any optimization flags.
To recover program structure in ``sweep3d`` and any shared libraries used during the run for use with |hpcprof(1)|, execute::

  hpcstruct hpctoolkit-sweep3d-measurements

The output is placed in a subdirectory of the measurements directory.

These program structure files are used to interpret performance measurements in ``hpctoolkit-sweep3d-measurements``::

  hpcprof hpctoolkit-sweep3d-measurements

2
---

Assume we have used HPCToolkit to collect performance measurements for the (optimized) GPU-accelerated CPU binary ``laghos``, which offloaded computation onto one or more Nvidia GPUs.
Assume that performance measurement data for the application is in the measurement directory ``hpctoolkit-laghos-measurements``.

Assume that the CPU code for ``laghos`` was compiled with debugging information using the ``-g`` compiler flag in addition to any optimization flags and that the GPU code the application contains was compiled with line map information (``-lineinfo``).

To recover program structure information for the ``laghos`` CPU binary, and any shared libraries it used during the run, as well as any GPU binaries it used, execute::

  hpcstruct hpctoolkit-laghos-measurements

The measurement directory will be augmented with program structure information recovered for the ``laghos`` binary, any shared libraries it used, and any GPU binaries it used.
All will be stored in subdirectories of the measurements directory::

  hpcprof hpctoolkit-laghos-measurements

NOTES
=====

1
---

For best results, an application binary should be compiled with debugging information.
To generate debugging information while also enabling optimizations, use the appropriate variant of ``-g`` for the following compilers:

- GNU compilers: ``-g``
- Intel compilers: ``-g -debug inline_debug_info``
- IBM compilers: ``-g -fstandalone-debug -qfulldebug -qfullpath``
- PGI compilers: ``-gopt``
- Nvidia's nvcc:

  - ``-lineinfo`` provides line mappings for optimized or unoptimized code
  - ``-G`` provides line mappings and inline information for unoptimized code

2
---

While hpcstruct attempts to guard against inaccurate debugging information, some compilers (notably PGI's) often generate invalid and inconsistent debugging information.
Garbage in; garbage out.

3
---

C++ mangling is compiler specific.
On non-GNU platforms, hpcstruct tries both platform's and GNU's demangler.

SEE ALSO
========

|hpctoolkit(1)|

.. |hpctoolkit(1)| replace:: **hpctoolkit**\(1)
.. |hpcprof(1)| replace:: **hpcprof**\(1)
