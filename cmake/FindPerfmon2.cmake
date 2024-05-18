# SPDX-FileCopyrightText: 2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

find_path(Perfmon2_INCLUDE_DIR
  NAMES perfmon/pfmlib.h)
find_library(Perfmon2_LIBRARY
  NAMES pfm)

if(Perfmon2_INCLUDE_DIR)
  file(READ "${Perfmon2_INCLUDE_DIR}/perfmon/pfmlib.h" header)
  string(REGEX MATCH "#define LIBPFM_VERSION[ \t]+\\([0-9]+ << 16 \\| [0-9]+\\)" macro "${header}")
  string(REGEX MATCH "\\([0-9]+ << 16 \\| [0-9]+\\)" macroFinal "${macro}")
  string(REGEX REPLACE "\\(([0-9]+) << 16 \\| ([0-9]+)\\)" "\\1.\\2" Perfmon2_VERSION "${macroFinal}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Perfmon2
  FOUND_VAR Perfmon2_FOUND
  REQUIRED_VARS
    Perfmon2_INCLUDE_DIR
    Perfmon2_LIBRARY
  VERSION_VAR Perfmon2_VERSION
)

if(Perfmon2_FOUND)
  add_library(Perfmon2::perfmon2 UNKNOWN IMPORTED)
  set_target_properties(Perfmon2::perfmon2 PROPERTIES
    IMPORTED_LOCATION "${Perfmon2_LIBRARY}")
  target_include_directories(Perfmon2::perfmon2 INTERFACE "${Perfmon2_INCLUDE_DIR}")
endif()

mark_as_advanced(
  Perfmon2_INCLUDE_DIR
  Perfmon2_LIBRARY
)
