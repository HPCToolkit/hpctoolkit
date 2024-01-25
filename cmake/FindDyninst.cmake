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
  if(TARGET ${comp})
    message(STATUS "Found Dyninst component: ${comp}")
    set(Dyninst_${comp}_FOUND TRUE)

    get_target_property(Dyninst_${comp}_IMPORTED_CONFIGS ${comp} IMPORTED_CONFIGURATIONS)
    message(STATUS "Dyninst::${comp} IMPORTED_CONFIGURATIONS: ${Dyninst_${comp}_IMPORTED_CONFIGS}")
    if(NOT Dyninst_${comp}_IMPORTED_CONFIGS)
      # Must be an unsuffixed library with no build information. This is highly irregular but we
      # do support this case for completeness.
      get_target_property(Dyninst_${comp}_LIBRARY ${comp} IMPORTED_LOCATION)
      message(STATUS "Dyninst::${comp} IMPORTED_LOCATION: ${Dyninst_${comp}_LIBRARY}")
      if(NOT Dyninst_${comp}_LIBRARY)
        set(Dyninst_${comp}_FOUND FALSE)
      endif()
    else()
      # Fetch the location for every possible suffix
      foreach(config IN LISTS Dyninst_${comp}_IMPORTED_CONFIGS)
        get_target_property(Dyninst_${comp}_LIBRARY_${config} ${comp} IMPORTED_LOCATION_${config})
        message(STATUS "Dyninst::${comp} IMPORTED_LOCATION_${config}: ${Dyninst_${comp}_LIBRARY_${config}}")
        if(NOT Dyninst_${comp}_LIBRARY_${config})
          set(Dyninst_${comp}_FOUND FALSE)
          break()
        endif()
      endforeach()
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
  target_include_directories("Dyninst::${comp}" INTERFACE "${DYNINST_INCLUDE_DIR}")

  if(NOT Dyninst_${comp}_IMPORTED_CONFIGS)
    set_target_properties("Dyninst::${comp}" PROPERTIES
      IMPORTED_LOCATION "${Dyninst_${comp}_LIBRARY}")
  else()
    foreach(config IN LISTS Dyninst_${comp}_IMPORTED_CONFIGS)
      set_property(TARGET "Dyninst::${comp}" APPEND PROPERTY IMPORTED_CONFIGURATIONS ${config})
      set_target_properties("Dyninst::${comp}" PROPERTIES
        IMPORTED_LOCATION_${config} "${Dyninst_${comp}_LIBRARY_${config}}")
    endforeach()
  endif()
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
