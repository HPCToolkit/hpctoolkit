# SPDX-FileCopyrightText: 2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

find_path(ROCtracer_INCLUDE_DIR
  NAMES roctracer/roctracer_hip.h
  PATH_SUFFIXES roctracer)
find_library(ROCtracer_LIBRARY
  NAMES roctracer64
  PATH_SUFFIXES roctracer)

if(ROCtracer_INCLUDE_DIR)
  file(READ "${ROCtracer_INCLUDE_DIR}/roctracer/roctracer.h" header)
  string(REGEX MATCH "#define ROCTRACER_VERSION_MAJOR [0-9]+" macroMajor "${header}")
  string(REGEX MATCH "[0-9]+" versionMajor "${macroMajor}")
  string(REGEX MATCH "#define ROCTRACER_VERSION_MINOR [0-9]+" macroMinor "${header}")
  string(REGEX MATCH "[0-9]+" versionMinor "${macroMinor}")
  set(ROCtracer_VERSION "${versionMajor}.${versionMinor}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ROCtracer
  FOUND_VAR ROCtracer_FOUND
  REQUIRED_VARS
    ROCtracer_INCLUDE_DIR
    ROCtracer_LIBRARY
  VERSION_VAR ROCtracer_VERSION
)

if(ROCtracer_FOUND)
  add_library(ROCtracer::ROCtracer UNKNOWN IMPORTED)
  set_target_properties(ROCtracer::ROCtracer PROPERTIES
    IMPORTED_LOCATION "${ROCtracer_LIBRARY}")
  target_include_directories(ROCtracer::ROCtracer INTERFACE "${ROCtracer_INCLUDE_DIR}")

  # In ROCm 5.1, ROCtracer also requires the roctracer/ subdirectory to be available on the
  # include paths. We use a compile test to determine if this is needed at all.
  include(CheckIncludeFile)
  check_include_file("roctracer/roctracer_hip.h" ROCtracer_GOOD_HEADERS
    CMAKE_REQUIRED_INCLUDES "${ROCtracer_INCLUDE_DIR}")
  if(NOT ROCtracer_GOOD_HEADERS)
    target_include_directories(ROCtracer::ROCtracer INTERFACE "${ROCtracer_INCLUDE_DIR}/roctracer")
  endif()
endif()

mark_as_advanced(
  ROCtracer_INCLUDE_DIR
  ROCtracer_LIBRARY
  ROCtracer_GOOD_HEADERS
)
