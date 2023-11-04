==========
HPCTOOLKIT
==========

:Date: 2021/09/11

NAME
====

**HPCToolkit** is a collection of performance analysis tools for
node-based performance analysis. It has been designed around the
following principles:

**\***
   **Be language independent.**

**\***
   **Avoid code instrumentation.**

**\***
   **Avoid blind spots.**

**\***
   **Provide context for understanding layered and object-oriented
   software.**

**\***
   **Support multiple performance measures to prevent myopic
   interpretation.**

**\***
   **Display user-defined derived performance metrics for effective
   analysis.**

**\***
   **Take a top down approach to performance analysis.**

**\***
   **Use hierarchical aggregation to mitigate approximate attribution.**

**\***
   **Ensure that measurement and analysis can scale to very large
   programs and executions.**

More detailed explanation of these design principles is available in
papers on the HPCToolkit website at hpctoolkit.org.

DESCRIPTION
===========

A typical performance analysis session consists of:

1.
   **Measuring execution costs.** *hpcrun*\ (1) uses statistical
   sampling to collect with low overhead and high accuracy a set of
   *call path profiles*, i.e. measurements of hardware resource
   consumption (costs) together with the call paths at which consumption
   occurred.

2.
   **Analyzing source code structure.** *hpcstruct*\ (1) discovers
   static program structure such as procedures and loop nests from
   binary code in the application's executable and the shared libraries
   and compiled GPU binaries. It takes into account optimizing compiler
   transformations such as restructuring of procedures and loops for
   inlining, software pipelining, multicore parallelization, and
   offloading to GPUs.

3.
   **Attributing measured costs to source code structure.**
   *hpcprof*\ (1) or *hpcprof-mpi*\ (1) combines hpcrun's dynamic
   profiles with hpcstruct's static program structure information to
   attribute measured costs incurred by the optimized object code to
   meaningful source code constructs such as procedures, loop nests, and
   individual lines of code. The result of attribution is an *experiment
   database* stored in a file system directory.

4.
   **Visualizing attributed costs in source code or timeline views.**
   *hpcviewer*\ (1) are tools for presenting the resulting experiment
   databases. hpcviewer displays measurements in outline form, each
   entry attributing costs to a source code construct by line number and
   linked to a display of corresponding application source code.
   hpcviewer displays measurements as a two dimensional timeline with
   execution progress aalong the horizontal axis and the application's
   parallel threads along the vertical axis. The visualization step may
   be done interactively with either tool on a personal computer. even
   if the application must run in batch on a large computing cluster. To
   this end, experiment databases are self contained and relocatable,
   even containing a copy of the application source code, and the
   hpcviewer tools is platform-independent (via Eclipse RCP) and
   lightweight enough for good interactive performance on a laptop.

EXAMPLES
========

Assume we have an application called zoo whose source code is located in
*path-to-zoo*. First compile and link your application normally with
full optimization and as much debugging information as possible.
Typically this involves compiler options such as -O3 -g. (See
*hpcstruct*\ (1) for options for specific compilers.) Then perform the
following steps.

1.
   **Measure**. Profile with *hpcrun*\ (1) . Assume you wish to measure
   two different sets of resources, which will require two measurement
   runs. hpcrun always collects the data needed for hpcviewer, but if
   you want to use traces you must add the -t / --trace option to
   collect additional data.

::

     hpcrun -t <event-set-1> zoo
     hpcrun -t <event-set-2> zoo

*hpcrun*\ (1) by default puts its results into a measurement directory
named hpctoolkit-appname-measurements, so the two sets of measurements
are combined automatically.

2.
   **Analyze**. Use *hpcstruct*\ (1) to discover program structure of
   the program and the shared libraries and GPU binaries it used during
   the run. Although hpcstruct has a number of advanced options, it is
   typically run with none. The hpctoolkit-appname-measurements
   directory is passed as the last argument.

By default the generated structure files are put into subirectories of
the measurements directory.

3.
   **Attribute**. Create an experiment database using *hpcprof*\ (1) or
   *hpcprof-mpi*\ (1) . (The version of *hpcprof*\ (1) or
   *hpcprof-mpi*\ (1) must match the version of *hpcrun*\ (1) .) Use the
   -I option to specify the location of zoo's source code. The
   measurement directory is specified as the last argument. By default
   the generated experiment database is named hpctoolkit-zoo-database.

::

     hpcprof -I path-to-zoo/+ hpctoolkit-zoo-measurements

4.
   **Visualize**. Visualize using *hpcviewer*\ (1) the experiment
   database in either source or timeline view, on any machine where
   you've copied the database:

::

     hpcviewer hpctoolkit-zoo-database

In hpcviewer you may also view "derived metrics", ie combinations of
measured metrics which are computed on the fly. See *The hpcviewer User
Interface* Guide for more information.

SEE ALSO
========

| *hpcrun*\ (1)
| *hpcstruct*\ (1)
| *hpcprof*\ (1) , *hpcprof-mpi*\ (1)
| *hpcviewer*\ (1)

VERSION
=======

Version: 2023.08.99-next

LICENSE AND COPYRIGHT
=====================

Copyright 
   (C)2002-2024, Rice University.

License 
   See LICENSE.

AUTHORS
=======

| Email: **hpctoolkit-forum =at= rice.edu**
| WWW: **http://hpctoolkit.org**.
