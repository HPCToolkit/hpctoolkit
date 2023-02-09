<!--
  This template is for proposals for refactors and other non-user-facing
  improvements to the HPCToolkit tools.

  Before you file, please make sure the title is short but specific.
    Good: `hpcrun: Use LD_AUDIT callbacks to track binaries loaded at runtime`
    Bad: `LD_AUDIT support`

  Fill in the sections below, and remove anything that isn't relevant to your
  particular feature request.
-->

<!--
  Summarize your proposed refactor. In general terms, what do you want changed?
-->

### Current Issues / Rationale
<!--
  Please detail any rationale for performing this refactor here. Why would it
  help develop or improve HPCToolkit moving forward? What development
  difficulties have you experienced that need to be solved?

  If this would help fix other bugs, link the bug reports here.
  If this change blocks development of a feature, link the feature request here.
-->

### Proposed Solution
<!--
  If you have an idea for a solution, detail it here. How do you want the
  internal APIs to behave? What language does it need to be written in?

  If other refactors are required before this can proceed, link those here.
-->

#### Affected Code Paths
<!--
  Please detail any code paths that would need to significantly change because
  of this refactor as a (possibly nested) bullet list. If possible, provide
  links to the source files that are most affected by this change.

  Example:
   - Load map collection, initialization order
     - [`main.c`](https://gitlab.com/hpctoolkit/hpctoolkit/-/blob/develop/src/tool/hpcrun/main.c)
   - Launch script
     - [`scripts/hpcrun.in`](https://gitlab.com/hpctoolkit/hpctoolkit/-/blob/develop/src/tool/hpcrun/scripts/hpcrun.in)
-->

### Additional Information
<!--
  Please provide any additional information that might spark discussion.
  Link any external sources or inspiration here, if possible.
-->

<!-- If only one tool would be affected, uncomment one of the following lines. -->
<!-- /label ~"component::hpcrun" -->
<!-- /label ~"component::hpcstruct" -->
<!-- /label ~"component::hpcprof" -->

<!-- Do not remove the following line. -->
/label ~"type::review"
