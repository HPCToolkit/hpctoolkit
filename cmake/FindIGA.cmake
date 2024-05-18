# SPDX-FileCopyrightText: 2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

find_path(IGA_INCLUDE_DIR
  NAMES iga/iga.h)
find_library(IGA_LIBRARY
  NAMES iga64)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IGA
  FOUND_VAR IGA_FOUND
  REQUIRED_VARS
    IGA_INCLUDE_DIR
    IGA_LIBRARY
)

if(IGA_FOUND)
  add_library(IGA::iga UNKNOWN IMPORTED)
  set_target_properties(IGA::iga PROPERTIES
    IMPORTED_LOCATION "${IGA_LIBRARY}")
  target_include_directories(IGA::iga INTERFACE "${IGA_INCLUDE_DIR}")
endif()

mark_as_advanced(
  IGA_INCLUDE_DIR
  IGA_LIBRARY
)
