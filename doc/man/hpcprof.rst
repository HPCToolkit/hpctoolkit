.. SPDX-License-Identifier: CC-BY-4.0
.. Copyright information is in the :copyright: field below

=======
hpcprof
=======
--------------------------------------------------------------------------------------------------------
analyzes call path profile performance measurements and attributes them to static source code structure.
--------------------------------------------------------------------------------------------------------

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

``hpcprof`` [*options*]... [``--``] *measurements*...

*mpi_launcher* [*mpi_options*]... ``hpcprof-mpi`` [*options*]... [``--``] *measurements*...

DESCRIPTION
===========

hpcprof analyzes call path profile performance measurements, attributes them to static source code structure, and generates an experiment database for use with |hpcviewer(1)|.

hpcprof-mpi additionally parallelizes the analysis across multiple compute nodes, using MPI for communication.
This is usually not needed except for especially large-scale executions (more than about 100,000 threads).
Using 8-10 compute nodes is recommended as larger numbers tend to not improve the analysis speed.

hpcprof/hpcprof-mpi expects a list of *measurements*, each of which is either a call path profile directory or an individual profile file.
hpcprof/hpcprof-mpi will find only source files whose paths are recorded in an application binary, shared library, or GPU binary as an absolute path still present in the file system or a relative path with respect to the current working directory.
If |hpcstruct(1)| was run on the measurements directory, no ``-S`` arguments are needed.

ARGUMENTS
=========

*measurements*...
  A sequence of file system paths, each specifying a call path profile directory or an individual profile file.

Default values for an option's optional arguments are shown in {}.

OPTIONS: INFORMATIONAL
----------------------

-v, --verbose  Print additional messages to stderr.
-V, --version  Print version information.
-h, --help  Print help.
-j threads  Perform analysis with *threads* threads. {<all available>}

OPTIONS: SOURCE CODE AND STATIC STRUCTURE
-----------------------------------------

--name name, --title name  Set the database's name (title) to *name*.

-S file, --structure file
  Use the structure file *file* produced by |hpcstruct(1)| to identify source code elements for attribution of performance.
  This option may be given multiple times, e.g. to provide structure for shared libraries in addition to the application executable.
  If |hpcstruct(1)| was run on the measurements directory, no such arguments are needed.

-R <old-path=new-path>, --replace-path <old-path=new-path>
  Replace every instance of *old-path* by *new-path* in all paths for which *old-path* is a prefix of a path to a binary measured by HPCToolkit or a source file used to produce a binary measured by HPCToolkit.
  Use a backslash (``\``) to escape instances of the equals sign (``=``) within a path.
  This option may be given multiple times.

  Use this option when a profile or binary contains references to files no longer present at their original path.
  For instance, a library may have been compiled by a system administrator and distributed with line map information containing file paths that point to a build directory that no longer exists.
  If you can locate a copy of the source code for the library, you can unpack it anywhere and provide an ``-R`` option that maps the prefix of the build directory to the prefix of the directory where you unpacked a copy of the library sources.

--only-exe filename
  Only include measurements for executables with the given filename.
  This option may be given multiple times to indicate multiple executables to include.

  Use this option when the application is measured through a wrapper script (e.g. ``hpcrun script.sh``) and you don't want to include the shell used to run the script in the resulting performance database.

OPTIONS: METRICS
----------------

-M metric, --metric metric
  Compute the specified metrics, where *metric* is one of the following:

  sum
    Sum over threads/processes

  stats
    Sum, Mean, StdDev (standard deviation), CoefVar (coefficient of variation), Min, Max over threads/processes

  thread
    per-thread/process metrics

  The default metric is sum.
  This option may be given multiple times.

OPTIONS: OUTPUT
---------------

-o dbpath, --db dbpath, --output dbpath
  Write the computed experiment database to *dbpath*.
  The default path is:

  .. parsed-literal::

    ./hpctoolkit-*application*-database

SEE ALSO
========

|hpctoolkit(1)|

.. |hpctoolkit(1)| replace:: **hpctoolkit**\(1)
.. |hpcviewer(1)| replace:: **hpcviewer**\(1)
.. |hpcstruct(1)| replace:: **hpcstruct**\(1)
