# SPDX-FileCopyrightText: 2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

find_path(Libiberty_INCLUDE_DIR
  # Libiberty upstream and Debian install the headers under .../include/libiberty,
  # but the RHELs install just demangle.h as .../include/demangle.h.
  # Accept either path, and we'll use __has_include later to skip over the issue.
  NAMES libiberty/demangle.h demangle.h)
find_library(Libiberty_LIBRARY
  NAMES iberty)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libiberty
  FOUND_VAR Libiberty_FOUND
  REQUIRED_VARS
    Libiberty_INCLUDE_DIR
    Libiberty_LIBRARY
)

if(Libiberty_FOUND)
  add_library(Libiberty::libiberty UNKNOWN IMPORTED)
  set_target_properties(Libiberty::libiberty PROPERTIES
    IMPORTED_LOCATION "${Libiberty_LIBRARY}")
  target_include_directories(Libiberty::libiberty INTERFACE "${Libiberty_INCLUDE_DIR}")
endif()

mark_as_advanced(
  Libiberty_INCLUDE_DIR
  Libiberty_LIBRARY
)
