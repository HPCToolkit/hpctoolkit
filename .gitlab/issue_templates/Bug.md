<!--
  This template is for bugs while using the HPCToolkit tools.

  Before you file, please make sure the title is short but specific.
    Good: `hpcprof sometimes deadlocks analyzing a renamed measurements with
           more than 3 threads`
    Bad: `Deadlock in parallel`

  Fill in the sections below, and remove anything that isn't relevant to your
  particular issue.
-->

<!--
  Summarize the bug here. What are you doing that isn't working as expected?
-->

### Setup
<!--
  Provide detailed instructions on how to set up the environment to reproduce
  the problem. Attach or link any source files needed to demonstrate the issue.
  If the OS or versions of any dependencies are important, say that here.

  Example:
  - Source file: [my-source.c](/uploads/...)
  - OS: Red Hat 8.9
  - Boost version 1.80.0
-->

<!-- Specify the version of HPCToolkit tested in the below code block. -->
```console
$ hpcrun --version
```

### To Reproduce
<!--
  Provide detailed steps to reproduce the issue as a numbered list. List any
  commands to run in `console` code blocks. Only include command output that is
  relevant to the reproduction of the issue.

  Example:
  1. Compile and measure the test application.
     ```console
     $ gcc -o my-app my-source.c
     $ hpcrun -e cycles ./my-app
     ```
  2. Rename the measurements directory.
     ```console
     $ mv hpctoolkit-my-app-measurements other-measurements
     ```
  3. Try to analyze the renamed measurements dir with threads.
     ```console
     $ hpcprof -j3 other-measurements
     ```
-->

### Erroneous Behavior
<!--
  What happened that was unexpected? How frequently does it happen?
  Why is this unexpected? Provide any relevant log output. If the output data
  is erroneous, provide an excerpt or screenshot of HPCViewer.
-->

### Additional Information
<!--
  If you have further information, such as full logs or output files, please
  attach or link to them here.
-->

### Root Cause
<!--
  If you have investigated the issue and have some insight into the root cause,
  please describe any insights here.
-->

<!-- Uncomment exactly one of the following lines, depending on which tool has the bug. -->
<!-- /label ~"component::hpcrun" -->
<!-- /label ~"component::hpcstruct" -->
<!-- /label ~"component::hpcprof" -->

<!-- Do not remove the following line. -->
/label ~"type::bug"
