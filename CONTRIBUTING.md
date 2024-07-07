<!--
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

# Contributing to HPCToolkit

All types of contributions are valued. See the list below for instructions and information about different ways to contribute. Please make sure to read the relevant section and any linked information in their entirety before making your contribution, it makes it at lot easier for us maintainers and smooths out the experience for everyone involved.

This project and everyone participating in it are governed by the [Linux Foundation Projects Code of Conduct](https://lfprojects.org/policies/code-of-conduct/).
By participating, you are expected to uphold this code.

We look forward to your involvement as a Contributor to the HPCToolkit Project.

- [I have a question!](#i-have-a-question)
- [I want to report a bug!](#i-want-to-report-a-bug)
- [Suggesting an enhancement or new feature](#suggesting-an-enhancement-or-feature)
- [Contributing changes](#contributing-changes)

## I have a question!

We're happy to answer! But before you ask, we ask that you:

1. Read through [the documentation]. The answer may be there, even if it's not very obvious. If it's confusing, feel free to [contribute an improvement to the documentation](#contributing-changes) to make it easier for others.

1. Search [our issue tracker on GitLab][gitlab issues]. There may be another issue that may help your query. If you do find one, feel free to leave a 👍 and/or a helpful comment to show your interest. Be sure to check for both open and closed issues.

1. Read or search the documentation from your sysadmins or for any involved tools/applications. There may be details that are causing your confusion.

If you are still confused or have unanswered questions, feel free to [open an issue](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/new?issuable_template=Question). Once your question is answered, we may ask you to [contribute an update the documentation](#contributing-changes) for others.

## I want to report a bug!

Something not working right? Before filing a bug, we ask that you:

1. Confirm you are running the latest released or development version of HPCToolkit. Given the project's limited manpower, we don't prioritize support of older releases.

1. Read any documentation from your sysadmins and ensure your environment is sane. Examples include loading incompatible modules from different authors, failing to set `LD_LIBRARY_PATH`, force-loading library dependencies incompatible with your build of HPCToolkit, etc.

1. Read through [the documentation]. If the bug is intended behavior, consider [asking a question](#i-have-a-question) about it or [suggesting an enhancement](#suggesting-an-enhancement-or-feature) to change it first.

1. Search [our issue tracker on GitLab][gitlab issues] for problems similar to yours. If you find one, please add a 👍 and/or a comment with your particular build parameters. Be sure to check both open and closed issues.

If you are still seeing unexpected behavior, please [open a new issue](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/new?issuable_template=Bug) and follow the tips & tricks below.

### Tips and tricks for writing a good bug report

1. Check if the issue still exists in the latest *development* version of HPCToolkit. This can be installed with Spack via:

   ```console
   $ spack uninstall -a hpctoolkit@develop
   $ spack install hpctoolkit@develop [+features...]
   ```

   See the [README.md](/README.md) for additional install options and instructions.

1. Provide any relevant context. Your OS, distribution, CPU architecture and versions of related dependencies are helpful (e.g. Ubuntu 22.04 x86-64, CUDA 12.5). Your hostname, shell, loaded modules or sysadmin's email address are not.

1. Try to reduce your example to the minimum reproducer and steps needed to trigger the issue. We cannot efficiently help if we don't have a way to easily recreate the failing scenario. Please avoid external code (e.g. `git clone my.cool/project`) so that we can adapt your reproducer into a regression test.

1. Give details for what happened that you didn't expect. If the output is unexpected, show the erroneous output and explain why you are certain it is wrong. If a tool crashed, provide a stack trace from `gdb`.

1. Upload any relevant log output you have. `hpcrun` generates `*.log` files in the output directory. `hpcstruct` and `hpcprof` produce logs on stdout/stderr.

1. Wait patiently for a response. It may take a while before we can get back to you.

## Suggesting an enhancement or feature

Need some extra features for HPCToolkit to work for you? Before you make a request, we ask that you:

1. Read through [the documentation]. We may already have the feature in a different form than you were initially expecting.

1. Search [our issue tracker on GitLab][gitlab issues] for requests for the same or similar features. If you find someone with the same use case, add a 👍 and/or a supportive comment to show your interest.

If you can't find any similar feature requests, feel free to [open a new issue](https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/new?issuable_template=Feature%20Request) and follow the tips & tricks below.

### Tips and tricks for writing a good feature request

1. Describe your use case / scenario in detail and how it applies to the community of users. We want to support the use cases of many users, including you.

1. Avoid [XY problems](https://en.wikipedia.org/wiki/XY_problem). Start by describing the use case (Y) instead of how you intend to solve it (X).

1. Explain how your proposed feature solves your use case. The worst outcome is that your feature gets implemented and it doesn't solve the original problem.

1. If possible, include a workaround that more-or-less achieves the main goal with no HPCToolkit changes. This helps other users get their work done without having to wait for the feature to be implemented.

1. Be open to alternative solutions that satisfy your use case. There are always multiple solutions to the same problem.

1. Wait a little before trying to implement your feature yourself. There may be design or other high-level issues that may drastically change how the code is implemented.

   > However, if you do implement it yourself, we ask that you assign yourself to the issue and [contribute your changes](#contributing-changes) back to HPCToolkit.

## Contributing Changes

Thanks for your efforts to improve HPCToolkit, and welcome to the community of Contributors to the HPCToolkit Project! There's a few things to keep in mind before and while contributing your changes, please read the sections below for details.

### The Legal Stuff

1. We require that all technical changes are made under the [BSD 3-Clause license](https://opensources.org/licenses/BSD-3-Clause) or under the [MIT license](https://opensource.org/license/mit). Documentation changes must be under the [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0). Data must be under the [CDLA-Permissive 2.0 license](https://cdla.dev/permissive-2.0).

1. The copyright and license must be specified by headers compatible with the [REUSE Specification](https://reuse.software). In most cases this is in a comment at the top of the file, for example:

   ```c
   // SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
   //
   // SPDX-License-Identifier: BSD-3-Clause
   ```

1. Copyrights should be made out to "Contributors to the HPCToolkit Project," meaning you! We do not require that you hand over your copyright rights to contribute.

1. We require that all technical contributions to HPCToolkit are accompanied with a [Developer Certificate of Origin](https://developercertificate.org) sign-off in every Git commit message. You can pass the `-s/--signoff` flag to `git commit` to generate this trailer, for example:

   ```
   Signed-off-by: Thomas Jefferson <thomas.jefferson@president.gov>
   ```

   > If your commits are missing this, you can edit the commit message and add the trailer with `git commit --amend -s`.\
   > For many commits, try:
   >
   > ```
   > git rebase -x 'git commit --amend -s --no-edit' develop
   > ```

1. Read over the [LICENSE.md](/LICENSE.md) for information on how your contribution will be licensed when distributed as part of HPCToolkit.

### Before you push your changes

1. If you are fixing a bug, please [report it](#i-want-to-report-a-bug) first. If you are adding a feature, please [write a request](#suggesting-an-enhancement-or-feature) first. In both cases search [our issue tracker][gitlab issues] for any other bugs/features related to your change. These issues help continuity if a bug reappears later or a feature needs redesigned, as well as for milestone/epic planning.

1. `git rebase` your commits onto the latest tip of `develop` (e.g. `git pull -r origin develop`). We will not accept MRs that contain merge commits.

1. Double-check that the author name and email on your commits is valid and matches your email on GitLab. This helps ensure your commits are properly attributed to you and to your GitLab account.

   > If your commits have the wrong author, set `git config user.name 'Thomas Jefferson'` and `git config user.email 'thomas.jefferson@president.gov'`, then replace the authorship on your commits with `git commit --amend --reset-author`.\
   > For many commits, try:
   >
   > ```
   > git rebase -x 'git commit --amend --reset-author --no-edit' develop
   > ```

### Submitting your first merge request (MR)

1. (If you haven't already,) [create a fork of the repository](https://gitlab.com/hpctoolkit/hpctoolkit/-/forks/new). This provides a place to store and share your changes.

1. `git push` your changes to your new fork. Name your Git branch something related to the goal of your changes.

1. [Create an MR](https://gitlab.com/hpctoolkit/hpctoolkit/-/merge_requests/new) for your changes. Feel free to mark it as a `Draft:` if you are still finalizing the details.

   > Include references to any related issues in your MR description, with phrases such as `Fixes #123`, `Resolves #456` or `Implements #789`. This [automatically closes the issues when the MR gets merged](https://docs.gitlab.com/ee/user/project/issues/managing_issues.html#closing-issues-automatically) and helps others find your changes.

1. Check and fix any errors seen in the CI for your MR.

1. Wait patently for a response and/or review. Be amenable to any requested changes.

### Tips and tricks for a successful MR

1. Follow best practices for the programming language. While we don't have a rigid coding style, consider reading and following the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) and the [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html).

1. Run [`pre-commit`](https://pre-commit.com/#install) locally before submitting the MR, either manually with `pre-commit run -a` or as part of the Git hooks installed by `pre-commit install`. CI will run the same linters/formatters and thus this (may) fail CI before your code even has a chance to run.

1. When possible, add a test that your changes are working as intended. Even simple "doesn't crash" tests find plenty of bugs.

1. Run the tests (`meson test`) at least once before submitting. The CI runs all the tests and will fail if any of them fail unexpectedly (or pass unexpectedly).

1. Update the Sphinx documentation (`doc/src/`) with your new changes. This is what makes your changes usable by users.

1. [Squash](https://stackoverflow.com/questions/5189560/how-do-i-squash-my-last-n-commits-together) your changes into a small and clean set of Git commits. Using multiple commits for related but separate changes is encouraged, e.g. a common CLI flag implemented in `hpcstruct` vs. `hpcprof`. Using multiple commits for entirely intermediate changes (e.g. multiple iterations of an API) is discouraged.

   > You may instead opt to enable the "squash commits when merge request is approved" checkbox. This is allowed, but we discourage this since any details in the commit messages are lost when inspecting the Git history (e.g. `git blame`).

1. Both before and after submitting your MR, fix any compiler warnings from your build and from CI. Compiler warnings are errors (or unexpected behaviors) waiting to happen, cleaning them up improves your code for the better.

[gitlab issues]: https://gitlab.com/hpctoolkit/hpctoolkit/-/issues
[the documentation]: https://hpctoolkit.gitlab.io/hpctoolkit
