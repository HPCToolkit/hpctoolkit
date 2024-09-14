
// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-debug.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define FORALL_ZE_RESULT(macro)\
  macro(ZE_RESULT_SUCCESS)\
  macro(ZE_RESULT_NOT_READY)\
  macro(ZE_RESULT_ERROR_DEVICE_LOST)\
  macro(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY)\
  macro(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY)\
  macro(ZE_RESULT_ERROR_MODULE_BUILD_FAILURE)\
  macro(ZE_RESULT_ERROR_MODULE_LINK_FAILURE)\
  macro(ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET)\
  macro(ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE)\
  macro(ZE_RESULT_EXP_ERROR_DEVICE_IS_NOT_VERTEX)\
  macro(ZE_RESULT_EXP_ERROR_VERTEX_IS_NOT_DEVICE)\
  macro(ZE_RESULT_EXP_ERROR_REMOTE_DEVICE)\
  macro(ZE_RESULT_EXP_ERROR_OPERANDS_INCOMPATIBLE)\
  macro(ZE_RESULT_EXP_RTAS_BUILD_RETRY)\
  macro(ZE_RESULT_EXP_RTAS_BUILD_DEFERRED)\
  macro(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS)\
  macro(ZE_RESULT_ERROR_NOT_AVAILABLE)\
  macro(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE)\
  macro(ZE_RESULT_WARNING_DROPPED_DATA)\
  macro(ZE_RESULT_ERROR_UNINITIALIZED)\
  macro(ZE_RESULT_ERROR_UNSUPPORTED_VERSION)\
  macro(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE)\
  macro(ZE_RESULT_ERROR_INVALID_ARGUMENT)\
  macro(ZE_RESULT_ERROR_INVALID_NULL_HANDLE)\
  macro(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE)\
  macro(ZE_RESULT_ERROR_INVALID_NULL_POINTER)\
  macro(ZE_RESULT_ERROR_INVALID_SIZE)\
  macro(ZE_RESULT_ERROR_UNSUPPORTED_SIZE)\
  macro(ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT)\
  macro(ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT)\
  macro(ZE_RESULT_ERROR_INVALID_ENUMERATION)\
  macro(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION)\
  macro(ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT)\
  macro(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY)\
  macro(ZE_RESULT_ERROR_INVALID_GLOBAL_NAME)\
  macro(ZE_RESULT_ERROR_INVALID_KERNEL_NAME)\
  macro(ZE_RESULT_ERROR_INVALID_FUNCTION_NAME)\
  macro(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION)\
  macro(ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION)\
  macro(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX)\
  macro(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE)\
  macro(ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE)\
  macro(ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED)\
  macro(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE)\
  macro(ZE_RESULT_ERROR_OVERLAPPING_REGIONS)\
  macro(ZE_RESULT_WARNING_ACTION_REQUIRED)\
  macro(ZE_RESULT_ERROR_UNKNOWN)\
  macro(ZE_RESULT_FORCE_UINT32)



//*****************************************************************************
// interface functions
//*****************************************************************************

const char *
ze_result_to_string
(
  ze_result_t result
)
{
#define ENUM_TO_STRING(x) case x: error = #x;
  const char *error;
  switch(result) {
    FORALL_ZE_RESULT(ENUM_TO_STRING);
    default: error = "ZE_RESULT_UNEXPECTED";
  }
#undef ENUM_TO_STRING

  return error;
}
