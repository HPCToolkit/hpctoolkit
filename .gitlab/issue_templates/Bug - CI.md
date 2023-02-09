<!--
  This template is for spurious failures with the CI infrastructure.

  Before you file, please make sure the title is short but specific, starts
  with `ci:`. If only one job failed, include the name of that job.
    Good: `ci: Failed in precommit, Python poetry error`
    Bad: `precommit failed`

  If the problem was caused by the Runner infrastructure (network or Podman
  failures, test timeouts, etc.), convert this issue into an "incident."

  Fill in the sections below, and remove anything that isn't relevant to your
  particular issue.
-->

### Failing Jobs/Pipelines
<!--
  Link to the failing jobs and/or pipelines.
-->
 - Job [#12345](https://gitlab.com/hpctoolkit/hpctoolkit/-/jobs/12345)

### Common Errors
<!--
  Copy any common error messages from the failing jobs in a code block.
-->

### Root Cause
<!--
  If you have investigated the issue and have some insight into the root cause,
  please describe any insights here.
-->

<!-- Do not remove the following lines. -->
/label ~"component::infrastructure"
/label ~"type::bug"
