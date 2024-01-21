find_path(Libmonitor_INCLUDE_DIR
  NAMES monitor.h)
find_library(Libmonitor_LIBRARY
  NAMES monitor)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libmonitor
  FOUND_VAR Libmonitor_FOUND
  REQUIRED_VARS
    Libmonitor_INCLUDE_DIR
    Libmonitor_LIBRARY
)

if(Libmonitor_FOUND)
  add_library(Libmonitor::libmonitor UNKNOWN IMPORTED)
  set_target_properties(Libmonitor::libmonitor PROPERTIES
    IMPORTED_LOCATION "${Libmonitor_LIBRARY}")
  target_include_directories(Libmonitor::libmonitor INTERFACE "${Libmonitor_INCLUDE_DIR}")
endif()

mark_as_advanced(
  Libmonitor_INCLUDE_DIR
  Libmonitor_LIBRARY
)
