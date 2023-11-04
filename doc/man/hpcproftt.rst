=========
hpcproftt
=========
-----------------------------------------------------------------
generates textual dumps of call path profiles recorded by hpcrun.
-----------------------------------------------------------------

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

SYNOPSIS
========

| ``hpcproftt`` *profile_file*...
| ``hpcproftt`` **-V**
| ``hpcproftt`` **-h**

DESCRIPTION
===========

hpcproftt generates textual dumps of call path profiles recorded by hpcrun.
The profile list may contain one or more call path profiles.

This tool principally intended for use by HPCToolkit developers rather than end users.
Under some circumstances, a user might be interested in examining the raw measurement data (1) if HPCToolkit's tool chain has a problem processing the measurement data, or (2) if he or she is interested in inspecting measurements data associated with an individual instruction, which is below the line-level attribution reported by HPCToolkit's graphical user interfaces.

The first section of the output describes the profile header.
The profile header contains information including the name of the executable, its path, and the value of the PATH environment variable when the program was run, along with other metadata including the batch scheduler job id, MPI rank, thread ID, host ID, process ID on the host, and times when the first and last samples were recorded if tracing was enabled during an execution.

The second section describes the epoch header.
Conceptually, measurement of an execution is divided into one or more epochs in time.
Measurements taken during an epoch are collected and flushed to disk separately.
This section is mostly of historical interest as now hpcrun typically collects measurements in a single epoch.

The third section of the profile reports the metric table.
This lists all metrics collected during the execution of an MPI rank or thread.
Information about a metric includes its name, type, format, and whether there is another partner metric.
The show and showPercent fields indicate whether the metrics should be displayed by default in hpcviewer.
The period field is used to scale the number of samples collected, e.g. to convert samples collected every 1M cache misses into cache miss counts.

The next section of the output reports information that HPCToolkit's collected about the application's load map during execution.
The load map consists of a series of pairs, where an executable or shared library is assigned an integer load module ID, known as an lm-id.
Call path samples consist of a sequence of (load module ID, offset) pairs.
Each pair represents a location in an application binary or one of its associated shared libraries.
Load module IDs present in a call path profile generally correspond to load module IDs in this table.
In certain cases, the special dummy load module ID 0 used to represent CCT nodes that are not associated with object code.
The calling context tree root is an example of one of these nodes.

The last component of the file is the Calling Context Tree (CCT).
Each node in the tree has an ID, the ID of its parent in the tree, an (lm-id, lm-ip) pair that represents the load module and a relative instruction pointer within that load module that represents a location in a call path.
A node in the tree may or may not have non-zero metric values associated with it.
If a node's ID is negative, that means the node is a leaf in the tree.
If a node's ID is odd, that means that it was recorded in an associated trace file and must be handled carefully during post-processing.

ARGUMENTS
=========

*profile_file*...
   A list of one or more call path profiles.

OPTIONS
-------

-V, --version  Print version information.
-h, --help  Print help.

SEE ALSO
========

|hpctoolkit(1)|

.. |hpctoolkit(1)| replace:: **hpctoolkit**\(1)
