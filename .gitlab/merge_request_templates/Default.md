<!--
  Before you file, please make sure the title is short but specific.
    Good: `hpcrun: Add support for measuring HIP applications`
    Bad: `HIP support`

  Fill in the sections below, and remove anything that isn't relevant to your
  particular MR.

  Thank you for helping improve HPCToolkit!
-->

<!--
  Edit the text below to describe in detail what your MR does and why. Provide
  links to any filed issues this MR closes or relates to. Issues this MR will
  close should be preceded by one of the following words to enable auto-closure:
   - Close, Closes, Closed, Closing, close, closes, closed, closing
   - Fix, Fixes, Fixed, Fixing, fix, fixes, fixed, fixing
   - Resolve, Resolves, Resolved, Resolving, resolve, resolves, resolved, resolving
   - Implement, Implements, Implemented, Implementing, implement, implements, implemented, implementing
  For details see [Issue Closing Pattern](https://docs.gitlab.com/ee/user/project/issues/managing_issues.html#default-closing-pattern).
-->

%{all_commits}

### To Demonstrate
<!--
  If this MR adds a new feature, provide a "tutorial" to the new functionality
  in a handful of detailed steps. Only include command output when it is
  relevant to the MR's changes. Provide a screenshot if needed.

  Example:
  1. Compile and measure a simple HIP application.
     ```console
     $ hipcc -o vecadd vecadd.hip.cpp
     ```
  2. Measure it with the new `-e gpu=amd` support.
     ``
     $ hpcrun -e gpu=amd ./vecadd
     ```
  3. Analyze the measurements. We have GPU metrics now!
     ```console
     $ hpcstruct hpctoolkit-vecadd-measurements
     $ hpcprof hpctoolkit-vecadd-measurements
     $ hpcviewer hpctoolkit-vecadd-database
     ```
     ![Viewer Screenshot](/uploads/...)
-->

### Backward Compatibility
<!--
  Does this MR alter the data formats (*.hpcrun, *.hpcstruct, *.db, etc.)?
  If so, detail how prior versions will (or won't) handle the newer version
  of the format(s) here. If not, uncomment the following line.
-->
<!-- - This MR does not alter the data formats. -->

<!--
  Does this MR alter the CLI for any of the tools? If so, detail any flags that
  have been deprecated and/or removed from the CLI, and their intended
  replacements. If not, uncomment the following line.
-->
<!-- - This MR does not alter any CLIs. -->

### Additional Information
<!--
  If you have any other context about this MR, please write that here.
-->

#### Checklist
<!--
  Please fill out the checklist below. Doing this helps encourage improvements
  to the overall quality of our code, as well as catches common issues early.

  If a line here is not appropriate for this MR, check the box,
  ~~strike out the line~~ and provide some rationale for skipping the check.
-->
 - [ ] I have run `pre-commit run -a` on my local checkout and fixed any
       reported issues.
 - [ ] I have confirmed that existing tests and/or the tests added by this MR
       cover the changes introduced by this MR.
 - [ ] I have confirmed that any added tests will be automatically run in CI.
 - [ ] I have updated all relevant documentation:
   - [ ] The `--help` text for the tools
   - [ ] The man page for the tools
   - [ ] The HPCToolkit User's Manual

<!-- If any of the below labels apply strongly to this MR, uncomment exactly one of the following lines. -->
<!-- /label ~"component::hpcrun" -->
<!-- /label ~"component::hpcstruct" -->
<!-- /label ~"component::hpcprof" -->
<!-- /label ~"component::dev" -->
<!-- /label ~"component::infrastructure" -->
