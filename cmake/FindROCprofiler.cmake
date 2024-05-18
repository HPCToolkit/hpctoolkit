# SPDX-FileCopyrightText: 2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

find_path(ROCprofiler_INCLUDE_DIR
  NAMES rocprofiler/rocprofiler.h
  PATH_SUFFIXES rocprofiler)
find_library(ROCprofiler_LIBRARY
  NAMES rocprofiler64
  PATH_SUFFIXES rocprofiler)
find_file(ROCprofiler_METRICS_XML
  NAMES rocprofiler/metrics.xml rocprofiler/lib/metrics.xml
  PATH_SUFFIXES lib lib/rocprofiler)
set(ROCprofiler_METRICS_XML "${ROCprofiler_METRICS_XML}")  # Quirky syntax needed to make dep.get_variable() work

if(ROCprofiler_INCLUDE_DIR)
  file(READ "${ROCprofiler_INCLUDE_DIR}/rocprofiler/rocprofiler.h" header)
  string(REGEX MATCH "#define ROCPROFILER_VERSION_MAJOR [0-9]+" macroMajor "${header}")
  string(REGEX MATCH "[0-9]+" versionMajor "${macroMajor}")
  string(REGEX MATCH "#define ROCPROFILER_VERSION_MINOR [0-9]+" macroMinor "${header}")
  string(REGEX MATCH "[0-9]+" versionMinor "${macroMinor}")
  set(ROCprofiler_VERSION "${versionMajor}.${versionMinor}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ROCprofiler
  FOUND_VAR ROCprofiler_FOUND
  REQUIRED_VARS
    ROCprofiler_INCLUDE_DIR
    ROCprofiler_LIBRARY
    ROCprofiler_METRICS_XML
  VERSION_VAR ROCprofiler_VERSION
)

if(ROCprofiler_FOUND)
  add_library(ROCprofiler::ROCprofiler UNKNOWN IMPORTED)
  set_target_properties(ROCprofiler::ROCprofiler PROPERTIES
    IMPORTED_LOCATION "${ROCprofiler_LIBRARY}")
  target_include_directories(ROCprofiler::ROCprofiler INTERFACE "${ROCprofiler_INCLUDE_DIR}")
endif()

mark_as_advanced(
  ROCprofiler_INCLUDE_DIR
  ROCprofiler_LIBRARY
  ROCprofiler_METRICS_XML
)
