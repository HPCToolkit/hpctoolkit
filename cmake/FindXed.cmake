# TODO: This should really be xed/xed-interface.h, but we're using the old include style, and
# that's also the only way Spack installs intel-xed at the moment. Sigh.
# Once https://github.com/spack/spack/issues/41268 is fixed, then this hack can be removed.
find_path(Xed_INCLUDE_DIR
  NAMES xed-interface.h
  PATH_SUFFIXES xed)
find_library(Xed_LIBRARY
  NAMES xed)

if(Xed_INCLUDE_DIR)
  file(READ "${Xed_INCLUDE_DIR}/xed-build-defines.h" header)
  string(REGEX MATCH "#[ \t]*define XED_VERSION[ \t]+\"v[0-9.]+\"" macro "${header}")
  string(REGEX MATCH "[0-9.]+" Xed_VERSION "${macro}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Xed
  FOUND_VAR Xed_FOUND
  REQUIRED_VARS
    Xed_INCLUDE_DIR
    Xed_LIBRARY
  VERSION_VAR Xed_VERSION
)

if(Xed_FOUND)
  add_library(Xed::xed UNKNOWN IMPORTED)
  set_target_properties(Xed::xed PROPERTIES
    IMPORTED_LOCATION "${Xed_LIBRARY}")
  target_include_directories(Xed::xed INTERFACE "${Xed_INCLUDE_DIR}")
endif()

mark_as_advanced(
  Xed_INCLUDE_DIR
  Xed_LIBRARY
)
