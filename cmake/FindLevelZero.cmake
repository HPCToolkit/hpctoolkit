find_path(LevelZero_INCLUDE_DIR
  NAMES ze_api.h
  PATH_SUFFIXES level_zero)
find_library(LevelZero_LIBRARY
  NAMES ze_loader)

if(LevelZero_INCLUDE_DIR)
  file(READ "${LevelZero_INCLUDE_DIR}/ze_api.h" header)
  string(REGEX MATCH "ZE_API_VERSION_CURRENT[ \t]+=[ \t]+ZE_MAKE_VERSION[(][^)]+[)]," macro "${header}")
  string(REGEX MATCHALL "[0-9]+" verlist "${macro}")
  list(JOIN verlist . LevelZero_VERSION)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LevelZero
  FOUND_VAR LevelZero_FOUND
  REQUIRED_VARS
    LevelZero_INCLUDE_DIR
    LevelZero_LIBRARY
  VERSION_VAR LevelZero_VERSION
)

if(LevelZero_FOUND)
  add_library(LevelZero::headers UNKNOWN IMPORTED)
  target_include_directories(LevelZero::headers INTERFACE "${LevelZero_INCLUDE_DIR}")

  add_library(LevelZero::levelzero UNKNOWN IMPORTED)
  set_target_properties(LevelZero::levelzero PROPERTIES
    IMPORTED_LOCATION "${LevelZero_LIBRARY}")
  target_link_libraries(LevelZero::levelzero INTERFACE LevelZero::headers)
endif()

mark_as_advanced(
  LevelZero_INCLUDE_DIR
  LevelZero_LIBRARY
)
