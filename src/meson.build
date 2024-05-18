# SPDX-FileCopyrightText: 2023-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

# Some environment modifications and extra dependencies are needed in order to make the tools work
# when run out of the build directory. These variables collect these details, and <tool>_test_*
# variables are the same for specific tools.
test_env = environment()
test_depends = []

subdir('common')

subdir('hpcrun')
subdir('hpcstruct')
subdir('hpcprof')

subdir('hpcproftt')
subdir('hpctracedump')

subdir('hpctesttool')
