find_path(Dyninst_INCLUDE_DIR
  NAMES Symtab.h CFG.h dyninstversion.h
  PATH_SUFFIXES dyninst)
find_library(Dyninst_common_LIBRARY common)
find_library(Dyninst_parseAPI_LIBRARY parseAPI)
find_library(Dyninst_instructionAPI_LIBRARY instructionAPI)
find_library(Dyninst_symtabAPI_LIBRARY symtabAPI)

mark_as_advanced(
  Dyninst_INCLUDE_DIR
  Dyninst_common_LIBRARY
  Dyninst_parseAPI_LIBRARY
  Dyninst_instructionAPI_LIBRARY
  Dyninst_symtabAPI_LIBRARY
)

find_package(Boost COMPONENTS atomic date_time filesystem system thread timer)
find_package(TBB COMPONENTS tbb tbbmalloc)

if(Dyninst_INCLUDE_DIR)
  file(READ "${Dyninst_INCLUDE_DIR}/dyninstversion.h" header)
  string(REGEX MATCH "#define DYNINST_MAJOR_VERSION [0-9]+" macroMajor "${header}")
  string(REGEX MATCH "[0-9]+" versionMajor "${macroMajor}")
  string(REGEX MATCH "#define DYNINST_MINOR_VERSION [0-9]+" macroMinor "${header}")
  string(REGEX MATCH "[0-9]+" versionMinor "${macroMinor}")
  string(REGEX MATCH "#define DYNINST_PATCH_VERSION [0-9]+" macroPatch "${header}")
  string(REGEX MATCH "[0-9]+" versionPatch "${macroPatch}")
  set(Dyninst_VERSION "${versionMajor}.${versionMinor}.${versionPatch}")
endif()

list(APPEND Dyninst_FIND_COMPONENTS common)

foreach(_comp IN LISTS Dyninst_FIND_COMPONENTS)
  if(_comp STREQUAL "common"
     OR _comp STREQUAL "parseAPI"
     OR _comp STREQUAL "instructionAPI"
     OR _comp STREQUAL "symtabAPI")
    if(EXISTS "${Dyninst_INCLUDE_DIR}" AND EXISTS "${Dyninst_${_comp}_LIBRARY}"
       AND EXISTS "${Dyninst_common_LIBRARY}" AND Boost_FOUND AND TBB_FOUND)
      set(Dyninst_${_comp}_FOUND TRUE)
    else()
      set(Dyninst_${_comp}_FOUND FALSE)
    endif()
  else()
    message(WARNING "${_comp} is not a valid Dyninst component")
    set(Dyninst_${_comp}_FOUND FALSE)
  endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Dyninst
  REQUIRED_VARS
    Dyninst_INCLUDE_DIR
    Dyninst_common_LIBRARY
    Dyninst_parseAPI_LIBRARY
    Dyninst_instructionAPI_LIBRARY
    Dyninst_symtabAPI_LIBRARY
  VERSION_VAR Dyninst_VERSION
  HANDLE_COMPONENTS
)

add_library(Dyninst::common UNKNOWN IMPORTED)
set_target_properties(Dyninst::common PROPERTIES
  IMPORTED_LOCATION "${Dyninst_common_LIBRARY}")
target_include_directories(Dyninst::common INTERFACE "${Dyninst_INCLUDE_DIR}")
target_link_libraries(Dyninst::common INTERFACE
    Boost::atomic Boost::date_time Boost::filesystem Boost::system Boost::thread
    TBB::tbb TBB::tbbmalloc)

if(Dyninst_parseAPI_FOUND)
  add_library(Dyninst::parseAPI UNKNOWN IMPORTED)
  set_target_properties(Dyninst::parseAPI PROPERTIES
    IMPORTED_LOCATION "${Dyninst_parseAPI_LIBRARY}")
  target_include_directories(Dyninst::parseAPI INTERFACE "${Dyninst_INCLUDE_DIR}")
  target_link_libraries(Dyninst::parseAPI INTERFACE
      Dyninst::common
      Boost::atomic Boost::date_time Boost::filesystem Boost::system Boost::thread
      TBB::tbb TBB::tbbmalloc)
endif()

if(Dyninst_instructionAPI_FOUND)
  add_library(Dyninst::instructionAPI UNKNOWN IMPORTED)
  set_target_properties(Dyninst::instructionAPI PROPERTIES
    IMPORTED_LOCATION "${Dyninst_instructionAPI_LIBRARY}")
  target_include_directories(Dyninst::instructionAPI INTERFACE "${Dyninst_INCLUDE_DIR}")
  target_link_libraries(Dyninst::instructionAPI INTERFACE
      Dyninst::common
      Boost::atomic;Boost::date_time Boost::filesystem Boost::system Boost::thread
      TBB::tbb TBB::tbbmalloc)
endif()

if(Dyninst_symtabAPI_FOUND)
  add_library(Dyninst::symtabAPI UNKNOWN IMPORTED)
  set_target_properties(Dyninst::symtabAPI PROPERTIES
    IMPORTED_LOCATION "${Dyninst_symtabAPI_LIBRARY}")
  target_include_directories(Dyninst::symtabAPI INTERFACE "${Dyninst_INCLUDE_DIR}")
  target_link_libraries(Dyninst::symtabAPI INTERFACE
      Dyninst::common
      Boost::atomic Boost::date_time Boost::filesystem Boost::system Boost::thread
      TBB::tbb TBB::tbbmalloc)
endif()
