find_path(IGCOpenCL_INCLUDE_DIR
  NAMES igc.opencl.h
  PATH_SUFFIXES igc)
find_path(IGCOpenCL_ExecutableFormat_INCLUDE_DIR
  NAMES patch_list.h
  HINTS "${IGCOpenCL_INCLUDE_DIR}/ocl_igc_shared/executable_format"
  PATH_SUFFIXES igc/ocl_igc_shared/executable_format)
find_path(IGCOpenCL_DeviceEnqueue_INCLUDE_DIR
  NAMES DeviceEnqueueInternalTypes.h
  HINTS "${IGCOpenCL_INCLUDE_DIR}/ocl_igc_shared/device_enqueue"
  PATH_SUFFIXES igc/ocl_igc_shared/device_enqueue)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IGCOpenCL
  FOUND_VAR IGCOpenCL_FOUND
  REQUIRED_VARS
    IGCOpenCL_INCLUDE_DIR
    IGCOpenCL_ExecutableFormat_INCLUDE_DIR
    IGCOpenCL_DeviceEnqueue_INCLUDE_DIR
)

if(IGCOpenCL_FOUND)
  add_library(IGCOpenCL::igcopencl UNKNOWN IMPORTED)
  target_include_directories(IGCOpenCL::igcopencl INTERFACE
    "${IGCOpenCL_INCLUDE_DIR}"
    "${IGCOpenCL_ExecutableFormat_INCLUDE_DIR}"
    "${IGCOpenCL_DeviceEnqueue_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  IGCOpenCL_INCLUDE_DIR
  IGCOpenCL_ExecutableFormat_INCLUDE_DIR
  IGCOpenCL_DeviceEnqueue_INCLUDE_DIR
)
