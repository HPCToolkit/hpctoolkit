#include "cupti-pc-sampling-api.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <hpcrun/main.h> // hpcrun_force_dlopen
#include <hpcrun/sample-sources/nvidia.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/messages/debug-flag.h>
#include <hpcrun/messages/messages.h>

#include "cupti-api.h"
#include "cupti-context-pc-sampling-map.h"

#define DEBUG 1

#define CUPTI_FN_NAME(f) DYN_FN_NAME(f)

#define CUPTI_FN(fn, args) \
  static CUptiResult (*CUPTI_FN_NAME(fn)) args

#define HPCRUN_CUPTI_PC_SAMPLING_CALL(fn, args) \
{ \
  CUptiResult status = CUPTI_FN_NAME(fn) args; \
  if (status != CUPTI_SUCCESS) { \
    const char *errstr; \
    cuptiGetResultString(status, &errstr); \
    fprintf(stderr, "error: function %s failed with error %s.\n", #fn, errstr); \
    exit(-1); \
  } \
}

#define FORALL_CUPTI_PC_SAMPLING_ROUTINES(macro) \
  macro(cuptiPCSamplingGetNumStallReasons) \
  macro(cuptiPCSamplingGetStallReasons) \
  macro(cuptiPCSamplingSetConfigurationAttribute) \
  macro(cuptiPCSamplingEnable) \
  macro(cuptiPCSamplingDisable) \
  macro(cuptiPCSamplingGetData)

CUPTI_FN
(
 cuptiPCSamplingGetNumStallReasons,
 (
  CUpti_PCSamplingGetNumStallReasonsParams *pParams
 )
);

CUPTI_FN
(
 cuptiPCSamplingGetStallReasons,
 (
  CUpti_PCSamplingGetStallReasonsParams *pParams
 )
);

CUPTI_FN
(
 cuptiPCSamplingSetConfigurationAttribute,
 (
  CUpti_PCSamplingConfigurationInfoParams *pParams
 )
);

CUPTI_FN
(
 cuptiPCSamplingGetData,
 (
  CUpti_PCSamplingGetDataParams *pParams
 )
);

CUPTI_FN
(
 cuptiPCSamplingEnable,
 (
  CUpti_PCSamplingEnableParams *pParams
 )
);

CUPTI_FN
(
 cuptiPCSamplingDisable,
 (
  CUpti_PCSamplingDisableParams *pParams
 )
);

#define HPCRUN_CUPTI_ACTIVITY_HW_BUFFER_SIZE (512 * 1024 * 1024)
#define HPCRUN_CUPTI_ACTIVITY_SW_BUFFER_SIZE (16 * 1024 * 1024)
#define HPCRUN_CUPTI_ACTIVITY_BUFFER_PC_NUM 2000
#define HPCRUN_CUPTI_ACTIVITY_USER_BUFFER_PC_NUM 2000

#define FORALL_CONFIG_INFO(macro) \
  macro(COLLECTION_MODE, 0) \
  macro(SAMPLING_PERIOD, 1) \
  macro(STALL_REASONS, 2) \
  macro(SAMPLING_BUFFER, 3) \
  macro(SCRATCH_BUFFER_SIZE, 4) \
  macro(HARDWARE_BUFFER_SIZE, 5) \
  macro(STOP_CONTROL, 6)

#define FORALL_CONFIG_INFO_COUNT(macro) \
  macro(CONFIG_INFO_COUNT, 7)

#define DECLARE_CONFIG_INFO(TYPE, VALUE) \
  TYPE = VALUE,

typedef enum {
  FORALL_CONFIG_INFO(DECLARE_CONFIG_INFO)
  FORALL_CONFIG_INFO_COUNT(DECLARE_CONFIG_INFO)
} config_info_t;


static void
collection_mode_config
(
 CUpti_PCSamplingCollectionMode collection_mode,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_COLLECTION_MODE;
  info->attributeData.collectionModeData.collectionMode = collection_mode;
}


static void
sampling_period_config
(
 uint32_t sampling_period,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SAMPLING_PERIOD;
  info->attributeData.samplingPeriodData.samplingPeriod = sampling_period;
}


static CUpti_PCSamplingData *
pc_sampling_data_alloc
(
 size_t num_pcs,
 size_t num_stall_reasons
)
{
  CUpti_PCSamplingData *buffer_pc = (CUpti_PCSamplingData *)calloc(1, sizeof(CUpti_PCSamplingData));
  buffer_pc->size = sizeof(CUpti_PCSamplingData);
  buffer_pc->collectNumPcs = num_pcs;
  buffer_pc->pPcData = (CUpti_PCSamplingPCData *)calloc(num_pcs, sizeof(CUpti_PCSamplingPCData));
  for (size_t i = 0; i < num_pcs; i++) {
    buffer_pc->pPcData[i].stallReason = (CUpti_PCSamplingStallReason *)calloc(
      num_stall_reasons, sizeof(CUpti_PCSamplingStallReason));
  }
  return buffer_pc;
}


static void
stall_reason_config
(
 CUcontext context,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  // Collect all stall reasons for now
  size_t num_stall_reasons = 0;
  CUpti_PCSamplingGetNumStallReasonsParams num_stall_reasons_params = {
    .size = CUpti_PCSamplingGetNumStallReasonsParamsSize,
    .ctx = context,
    .numStallReasons = &num_stall_reasons,
    .pPriv = NULL
  };
  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingGetNumStallReasons, (&num_stall_reasons_params));

  char **stall_reason_names = calloc(num_stall_reasons, sizeof(char *));
  for (size_t i = 0; i < num_stall_reasons; i++)
  {
    stall_reason_names[i] = calloc(STALL_REASON_STRING_SIZE, sizeof(char));
  }
  uint32_t *stall_reason_index = calloc(num_stall_reasons, sizeof(uint32_t));

  CUpti_PCSamplingGetStallReasonsParams stall_reasons_params = {
    .size = CUpti_PCSamplingGetStallReasonsParamsSize,
    .ctx = context,
    .numStallReasons = num_stall_reasons,
    .stallReasonIndex = stall_reason_index,
    .stallReasons = stall_reason_names,
    .pPriv = NULL
  };
  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingGetStallReasons, (&stall_reasons_params));

  info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_STALL_REASON;
  info->attributeData.stallReasonData.stallReasonCount = num_stall_reasons;
  info->attributeData.stallReasonData.pStallReasonIndex = stall_reason_index;

  CUpti_PCSamplingData *buffer_pc = pc_sampling_data_alloc(HPCRUN_CUPTI_ACTIVITY_BUFFER_PC_NUM, num_stall_reasons);
  CUpti_PCSamplingData *user_buffer_pc = pc_sampling_data_alloc(HPCRUN_CUPTI_ACTIVITY_USER_BUFFER_PC_NUM, num_stall_reasons);

  cupti_context_pc_sampling_map_insert(context, num_stall_reasons, stall_reason_index,
    stall_reason_names, HPCRUN_CUPTI_ACTIVITY_BUFFER_PC_NUM, buffer_pc,
    HPCRUN_CUPTI_ACTIVITY_USER_BUFFER_PC_NUM, user_buffer_pc);
}


static void
sampling_buffer_config
(
 CUcontext context,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  // User buffer to hold collected PC Sampling data in PC-To-Counter format
  cupti_context_pc_sampling_map_entry_t *entry = cupti_context_pc_sampling_map_lookup(context);
  void *buffer = cupti_context_pc_sampling_map_entry_buffer_pc_get(entry);
  
  if (buffer != NULL) {
    TMSG(CUPTI, "Set sampling data buffer");
    info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SAMPLING_DATA_BUFFER;
    info->attributeData.samplingDataBufferData.samplingDataBuffer = (void *)buffer;
  }
}


static void
scratch_buffer_size_config
(
 size_t scratch_buffer_size,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SCRATCH_BUFFER_SIZE;
  info->attributeData.scratchBufferSizeData.scratchBufferSize = scratch_buffer_size;
}


static void
hardware_buffer_size_config
(
 size_t hardware_buffer_size,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_HARDWARE_BUFFER_SIZE;
  info->attributeData.hardwareBufferSizeData.hardwareBufferSize = hardware_buffer_size;
}


static void
start_stop_control_config
(
 bool enable_start_stop_control,
 CUpti_PCSamplingConfigurationInfo *info
)
{
 info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_ENABLE_START_STOP_CONTROL;
 info->attributeData.enableStartStopControlData.enableStartStopControl = enable_start_stop_control;
}


void
cupti_pc_sampling_config
(
 CUcontext context,
 int frequency
)
{
  CUpti_PCSamplingConfigurationInfo *config_info = (CUpti_PCSamplingConfigurationInfo *)
    calloc(CONFIG_INFO_COUNT, sizeof(CUpti_PCSamplingConfigurationInfo));
    
  CUpti_PCSamplingConfigurationInfo *collection_mode_info = &config_info[COLLECTION_MODE];
  CUpti_PCSamplingConfigurationInfo *sampling_period_info = &config_info[SAMPLING_PERIOD];
  CUpti_PCSamplingConfigurationInfo *stall_reason_info = &config_info[STALL_REASONS];
  CUpti_PCSamplingConfigurationInfo *sampling_buffer_info = &config_info[SAMPLING_BUFFER];
  CUpti_PCSamplingConfigurationInfo *scratch_buffer_size_info = &config_info[SCRATCH_BUFFER_SIZE];
  CUpti_PCSamplingConfigurationInfo *hw_buffer_size_info = &config_info[HARDWARE_BUFFER_SIZE];
  CUpti_PCSamplingConfigurationInfo *start_stop_control_info = &config_info[STOP_CONTROL];

  // TODO(Keren): control all slots using control knobs
  collection_mode_config(CUPTI_PC_SAMPLING_COLLECTION_MODE_CONTINUOUS, collection_mode_info);

  sampling_period_config(cupti_pc_sampling_frequency_get(), sampling_period_info);

  stall_reason_config(context, stall_reason_info);

  sampling_buffer_config(context, sampling_buffer_info);

  scratch_buffer_size_config(HPCRUN_CUPTI_ACTIVITY_SW_BUFFER_SIZE, scratch_buffer_size_info);

  hardware_buffer_size_config(HPCRUN_CUPTI_ACTIVITY_HW_BUFFER_SIZE, hw_buffer_size_info);

  start_stop_control_config(false, start_stop_control_info);

  CUpti_PCSamplingConfigurationInfoParams info_params = {
    .size = CUpti_PCSamplingConfigurationInfoParamsSize,
    .pPriv = NULL,
    .ctx = context,
    .numAttributes = CONFIG_INFO_COUNT,
    .pPCSamplingConfigurationInfo = config_info
  };

  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingSetConfigurationAttribute, (&info_params));
}


void
cupti_pc_sampling_collect
(
 CUcontext context
)
{
  cupti_context_pc_sampling_map_entry_t *entry = cupti_context_pc_sampling_map_lookup(context);
  CUpti_PCSamplingData *buffer_pc = cupti_context_pc_sampling_map_entry_buffer_pc_get(entry);
  CUpti_PCSamplingData *user_buffer_pc = cupti_context_pc_sampling_map_entry_user_buffer_pc_get(entry);

  if (user_buffer_pc != NULL && buffer_pc != NULL) {
    TMSG(CUPTI, "NEW CUPTI totalNumPcs: %lu, totalSamples: %lu, collectNumPcs: %lu, remainingNumPcs: %lu",
      buffer_pc->totalNumPcs, buffer_pc->totalSamples, buffer_pc->collectNumPcs, buffer_pc->remainingNumPcs);

    if (DEBUG) {
      for (size_t i = 0; i < buffer_pc->totalNumPcs; ++i) {
        CUpti_PCSamplingPCData *pc_data = &buffer_pc->pPcData[i];
        TMSG(CUPTI, "cubinCrc: %lu, functionName: %s, pc: %lu, pcOffset: %u", pc_data->cubinCrc,
          pc_data->functionName, pc_data->pc, pc_data->pcOffset);
      }
    }

    while (buffer_pc->remainingNumPcs >= HPCRUN_CUPTI_ACTIVITY_USER_BUFFER_PC_NUM) {
      // Need to copy back buffer now, otherwise buffer_pc is not big enough to hold all samples
      CUpti_PCSamplingGetDataParams params = {
        .size = CUpti_PCSamplingGetDataParamsSize,
        .ctx = context,
        .pcSamplingData = user_buffer_pc,
        .pPriv = NULL
      };

      HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingGetData, (&params));

      if (DEBUG) {
        TMSG(CUPTI, "NEW CUPTI totalNumPcs: %lu, totalSamples: %lu, collectNumPcs: %lu, remainingNumPcs: %lu",
          user_buffer_pc->totalNumPcs, user_buffer_pc->totalSamples, user_buffer_pc->collectNumPcs, user_buffer_pc->remainingNumPcs);

        for (size_t i = 0; i < user_buffer_pc->totalNumPcs; ++i) {
          CUpti_PCSamplingPCData *pc_data = &user_buffer_pc->pPcData[i];
          TMSG(CUPTI, "cubinCrc: %lu, functionName: %s, pc: %lu, pcOffset: %u", pc_data->cubinCrc,
            pc_data->functionName, pc_data->pc, pc_data->pcOffset);
        }
      }
    }
  }
}


int
cupti_pc_sampling_bind
(
)
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(cupti, cupti_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define CUPTI_PC_SAMPLING_BIND(fn) \
  CHK_DLSYM(cupti, fn);

  FORALL_CUPTI_PC_SAMPLING_ROUTINES(CUPTI_PC_SAMPLING_BIND)

#undef CUPTI_PC_SAMPLING_BIND

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK

}


void
cupti_pc_sampling_enable2
(
 CUcontext context
)
{
  CUpti_PCSamplingEnableParams params = {
    .size = CUpti_PCSamplingEnableParamsSize,
    .ctx = context,
    .pPriv = NULL
  };
  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingEnable, (&params));
}


void
cupti_pc_sampling_disable2
(
 CUcontext context
)
{
  CUpti_PCSamplingDisableParams params = {
    .size = CUpti_PCSamplingDisableParamsSize,
    .ctx = context,
    .pPriv = NULL
  };
  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingDisable, (&params));
}


void
cupti_context_pc_sampling_flush
(
 CUcontext context
)
{
  cupti_pc_sampling_disable2(context);
  // Last collect
  cupti_pc_sampling_collect(context);
}


void
cupti_pc_sampling_flush
(
)
{
  cupti_context_pc_sampling_map_flush(cupti_context_pc_sampling_flush);
}
