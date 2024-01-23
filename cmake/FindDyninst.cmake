find_package(Dyninst CONFIG QUIET
  OPTIONAL_COMPONENTS ${Dyninst_FIND_COMPONENTS}
  VERSION ${Dyninst_FIND_VERSION})
find_package(Boost COMPONENTS atomic date_time filesystem system thread timer)
find_package(TBB COMPONENTS tbb tbbmalloc)

if(parseAPI IN_LIST Dyninst_FIND_COMPONENTS)
  list(APPEND Dyninst_FIND_COMPONENTS common instructionAPI symtabAPI)
endif()
if(symtabAPI IN_LIST Dyninst_FIND_COMPONENTS)
  list(APPEND Dyninst_FIND_COMPONENTS common dynElf dynDwarf)
endif()
if(instructionAPI IN_LIST Dyninst_FIND_COMPONENTS)
  list(APPEND Dyninst_FIND_COMPONENTS common)
endif()
if(dynDwarf IN_LIST Dyninst_FIND_COMPONENTS)
  list(APPEND Dyninst_FIND_COMPONENTS common dynElf)
endif()
if(dynElf IN_LIST Dyninst_FIND_COMPONENTS)
  list(APPEND Dyninst_FIND_COMPONENTS common)
endif()
list(REMOVE_DUPLICATES Dyninst_FIND_COMPONENTS)

foreach(comp IN LISTS Dyninst_FIND_COMPONENTS)
  if(TARGET "${comp}")
    message(STATUS "Found Dyninst component: ${comp}")
    set("Dyninst_${comp}_FOUND" TRUE)

    get_target_property("Dyninst_${comp}_LIBRARY_RELEASE" "${comp}" IMPORTED_LOCATION_RELEASE)
    message(STATUS "IMPORTED_LOCATION_RELEASE for Dyninst::${comp}: ${Dyninst_${comp}_LIBRARY_RELEASE}")
    get_target_property("Dyninst_${comp}_LIBRARY_DEBUG" "${comp}" IMPORTED_LOCATION_DEBUG)
    message(STATUS "IMPORTED_LOCATION_DEBUG for Dyninst::${comp}: ${Dyninst_${comp}_LIBRARY_DEBUG}")

    if((NOT EXISTS "${Dyninst_${comp}_LIBRARY_RELEASE}") AND (NOT EXISTS "${Dyninst_${comp}_LIBRARY_DEBUG}"))
      get_target_property("Dyninst_${comp}_LIBRARY" "${comp}" IMPORTED_LOCATION)
      message(STATUS "IMPORTED_LOCATION for Dyninst::${comp}: ${Dyninst_${comp}_LIBRARY}")

      if(NOT EXISTS "${Dyninst_{$comp}_LIBRARY}")
        set("Dyninst_${comp}_FOUND" FALSE)
      endif()
    endif()
  else()
    message(STATUS "Did NOT find Dyninst component: ${comp}")
    set("Dyninst_${comp}_FOUND" FALSE)
  endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Dyninst
  REQUIRED_VARS
    Dyninst_FOUND
    DYNINST_INCLUDE_DIR
    Boost_FOUND
    TBB_FOUND
  VERSION_VAR Dyninst_VERSION
  HANDLE_COMPONENTS
)

foreach(comp IN LISTS Dyninst_FIND_COMPONENTS)
  add_library("Dyninst::${comp}" UNKNOWN IMPORTED)

  if(EXISTS "${Dyninst_${comp}_LIBRARY}")
    set_target_properties("Dyninst::${comp}" PROPERTIES
      IMPORTED_LOCATION "${Dyninst_${comp}_LIBRARY}")
  endif()
  if(EXISTS "${Dyninst_${comp}_LIBRARY_RELEASE}")
    set_target_properties("Dyninst::${comp}" PROPERTIES
      IMPORTED_LOCATION_RELEASE "${Dyninst_${comp}_LIBRARY_RELEASE}")
    set_property(TARGET "Dyninst::${comp}" APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
  endif()
  if(EXISTS "${Dyninst_${comp}_LIBRARY_DEBUG}")
    set_target_properties("Dyninst::${comp}" PROPERTIES
      IMPORTED_LOCATION_DEBUG "${Dyninst_${comp}_LIBRARY_DEBUG}")
    set_property(TARGET "Dyninst::${comp}" APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
  endif()

  target_include_directories("Dyninst::${comp}" INTERFACE "${DYNINST_INCLUDE_DIR}")
endforeach()

if(TARGET Dyninst::common)
  target_link_libraries(Dyninst::common INTERFACE Boost::headers TBB::tbb)
endif()
if(TARGET Dynint::dynElf)
  target_link_libraries(Dyninst::dynElf INTERFACE Dyninst::common)
endif()
if(TARGET Dyninst::dynDwarf)
  target_link_libraries(Dyninst::dynDwarf INTERFACE Dyninst::common)
endif()
if(TARGET Dyninst::instructionAPI)
  target_link_libraries(Dyninst::instructionAPI INTERFACE
    Dyninst::common Dyninst::dynElf Dyninst::dynDwarf)
endif()
if(TARGET Dyninst::symtabAPI)
  target_link_libraries(Dyninst::symtabAPI INTERFACE
    Dyninst::common Dyninst::dynElf Dyninst::dynDwarf)
endif()
if(TARGET Dyninst::parseAPI)
  target_link_libraries(Dyninst::parseAPI INTERFACE
    Dyninst::common Dyninst::instructionAPI Dyninst::symtabAPI)
endif()
