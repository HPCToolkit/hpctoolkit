#include "cupti-pc-sampling-api.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <hpcrun/main.h> // hpcrun_force_dlopen
#include <hpcrun/sample-sources/nvidia.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/messages/debug-flag.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/control-knob.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>
#include <hpcrun/cct/cct.h>

#include "../gpu/gpu-operation-item.h"
#include "../gpu/gpu-operation-item-process.h"
#include "../gpu/gpu-operation-multiplexer.h"
#include "../gpu/gpu-range.h"
#include "../gpu/gpu-metrics.h"

#include "cuda-api.h"
#include "cubin-crc-map.h"
#include "cupti-api.h"
#include "cupti-context-map.h"
#include "cupti-range.h"
#include "cupti-pc-sampling-data.h"

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
  macro(cuptiPCSamplingGetData) \
  macro(cuptiPCSamplingStart) \
  macro(cuptiPCSamplingStop) \
  macro(cuptiGetCubinCrc)

CUPTI_FN
(
 cuptiGetCubinCrc,
 (
  CUpti_GetCubinCrcParams *pParams
 );
);

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

CUPTI_FN
(
 cuptiPCSamplingStart,
 (
  CUpti_PCSamplingStartParams *pParams
 )
);

CUPTI_FN
(
 cuptiPCSamplingStop,
 (
  CUpti_PCSamplingStopParams *pParams
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


static bool pc_sampling_active = false;

//******************************************************************************
// internal operations
//******************************************************************************

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
    stall_reason_names[i] = calloc(CUPTI_STALL_REASON_STRING_SIZE, sizeof(char));
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

  cupti_pc_sampling_data_t *pc_sampling_data =
    cupti_pc_sampling_data_produce(HPCRUN_CUPTI_ACTIVITY_BUFFER_PC_NUM, num_stall_reasons);

  cupti_context_map_pc_sampling_insert(context, num_stall_reasons, stall_reason_index,
    stall_reason_names, HPCRUN_CUPTI_ACTIVITY_BUFFER_PC_NUM, pc_sampling_data);
}


static void
sampling_buffer_config
(
 CUcontext context,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  // User buffer to hold collected PC Sampling data in PC-To-Counter format
  cupti_context_map_entry_t *entry = cupti_context_map_lookup(context);
  assert(entry != NULL);
  cupti_pc_sampling_data_t *pc_sampling_data = cupti_context_map_entry_pc_sampling_data_get(entry);
  CUpti_PCSamplingData *buffer_pc = cupti_pc_sampling_buffer_pc_get(pc_sampling_data);

  if (buffer_pc != NULL) {
    info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SAMPLING_DATA_BUFFER;
    info->attributeData.samplingDataBufferData.samplingDataBuffer = (void *)buffer_pc;
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
 uint32_t enable_start_stop_control,
 CUpti_PCSamplingConfigurationInfo *info
)
{
  info->attributeType = CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_ENABLE_START_STOP_CONTROL;
  info->attributeData.enableStartStopControlData.enableStartStopControl = enable_start_stop_control;
}


static void
pc_sampling2_translate
(
 void *pc_sampling_data,
 uint64_t total_num_pcs,
 uint32_t period,
 uint32_t range_id,
 cct_node_t *cct_node
)
{
  cupti_pc_sampling_data_t *data = (cupti_pc_sampling_data_t *)pc_sampling_data;
  CUpti_PCSamplingData *buffer_pc = cupti_pc_sampling_buffer_pc_get(data);

  uint64_t sample_period = 1 << period;
  uint64_t index;
  for (index = 0; index < total_num_pcs; ++index) {
    CUpti_PCSamplingPCData *pc_data = &buffer_pc->pPcData[index];
    ip_normalized_t pc = cubin_crc_transform(pc_data->cubinCrc, pc_data->functionIndex, pc_data->pcOffset);
    cct_node_t *cct_child = hpcrun_cct_insert_ip_norm(cct_node, pc, false);

    TMSG(CUPTI_ACTIVITY, "cubinCrc: %lu, lm_id: %u, lm_ip %p, functionName: %s, pcOffset: %p, count: %u", pc_data->cubinCrc,
      pc.lm_id, pc.lm_ip, pc_data->functionName, pc_data->pcOffset, pc_data->stallReasonCount);

    gpu_activity_t gpu_activity;
    gpu_activity.kind = GPU_ACTIVITY_PC_SAMPLING2;
    gpu_activity.cct_node = cct_child;
    gpu_activity.range_id = range_id;

    for (size_t j = 0; j < pc_data->stallReasonCount; ++j) {
      CUpti_PCSamplingStallReason *stall_reason = &pc_data->stallReason[j];
      uint32_t stall_reason_index = stall_reason->pcSamplingStallReasonIndex;
      // The first two stall reasons are count and drop, which we don't use
      if (stall_reason_index < 2) {
        continue;
      }
      stall_reason_index -= 2;

      uint64_t samples = stall_reason->samples * sample_period;
      uint32_t st_index = stall_reason_index / 2 + 1;

      if (samples > 0) {
        if (stall_reason_index % 2 == 0) {
          // non-latency sample
          gpu_activity.details.pc_sampling2.samples = samples;
          gpu_activity.details.pc_sampling2.latencySamples = 0;
        } else {
          // latency sample 
          gpu_activity.details.pc_sampling2.samples = 0;
          gpu_activity.details.pc_sampling2.latencySamples = samples;
        }

        gpu_activity.details.pc_sampling2.stallReason = st_index;
        gpu_metrics_attribute(&gpu_activity);
      }
    }
  }
}


static void
pc_sampling_activity_set
(
 gpu_activity_t *activity,
 uint32_t range_id,
 uint32_t context_id,
 cct_node_t *cct_node,
 cupti_pc_sampling_data_t *pc_sampling_data
)
{
  CUpti_PCSamplingData *buffer_pc = cupti_pc_sampling_buffer_pc_get(pc_sampling_data);

  activity->kind = GPU_ACTIVITY_PC_SAMPLING_INFO2;
  activity->range_id = range_id;
  activity->cct_node = cct_node;
  activity->details.pc_sampling_info2.context_id = context_id;
  activity->details.pc_sampling_info2.droppedSamples = buffer_pc->droppedSamples;
  activity->details.pc_sampling_info2.samplingPeriodInCycles = cupti_pc_sampling_frequency_get();
  activity->details.pc_sampling_info2.totalSamples = buffer_pc->totalSamples;
  activity->details.pc_sampling_info2.totalNumPcs = buffer_pc->totalNumPcs;
  activity->details.pc_sampling_info2.pc_sampling_data = pc_sampling_data;
  activity->details.pc_sampling_info2.translate = pc_sampling2_translate;
  activity->details.pc_sampling_info2.free = cupti_pc_sampling_data_free;
}

static void
pc_sampling_data_debug
(
 CUpti_PCSamplingData *buffer_pc,
 size_t num_stall_reasons
)
{
  TMSG(CUPTI_ACTIVITY, "totalNumPcs: %lu, totalSamples: %lu, collectNumPcs: %lu, remainingNumPcs: %lu",
    buffer_pc->totalNumPcs, buffer_pc->totalSamples, buffer_pc->collectNumPcs, buffer_pc->remainingNumPcs);

  for (size_t i = 0; i < buffer_pc->totalNumPcs; ++i) {
    CUpti_PCSamplingPCData *pc_data = &buffer_pc->pPcData[i];
    TMSG(CUPTI_ACTIVITY, "cubinCrc: %lu, functionName: %s, pcOffset: %u, count: %u", pc_data->cubinCrc,
      pc_data->functionName, pc_data->pcOffset, pc_data->stallReasonCount);

    for (size_t j = 0; j < pc_data->stallReasonCount; ++j) {
      if (pc_data->stallReason[j].samples > 0) {
        TMSG(CUPTI_ACTIVITY, "stall index: %u, count: %d", pc_data->stallReason[j].pcSamplingStallReasonIndex,
          pc_data->stallReason[j].samples);
      }
    }
  }
}


void
cupti_pc_sampling_start
(
 CUcontext context
)
{
  uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  TMSG(CUPTI, "PC sampling context_id %u start", context_id);

  pc_sampling_active = true;

  CUpti_PCSamplingStartParams params = {
    .size = CUpti_PCSamplingStartParamsSize,
    .ctx = context,
    .pPriv = NULL
  };
  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingStart, (&params));
}


void
cupti_pc_sampling_stop
(
 CUcontext context
)
{
  uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  TMSG(CUPTI, "PC sampling context_id %u stop", context_id);

  pc_sampling_active = false;

  CUpti_PCSamplingStopParams params = {
    .size = CUpti_PCSamplingStopParamsSize,
    .ctx = context,
    .pPriv = NULL
  };
  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingStop, (&params));
}


void
pc_sampling_collect
(
 uint32_t range_id,
 cct_node_t *cct_node,
 CUcontext context
)
{
  cupti_context_map_entry_t *entry = cupti_context_map_lookup(context);
  if (entry == NULL) {
    // PC sampling not enabled
    return;
  }

  uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  TMSG(CUPTI_TRACE, "PC sampling collect context_id %u, range_id %u", context_id, range_id);

  cupti_pc_sampling_data_t *device_pc_sampling_data = cupti_context_map_entry_pc_sampling_data_get(entry);
  CUpti_PCSamplingData *buffer_pc = cupti_pc_sampling_buffer_pc_get(device_pc_sampling_data);
  size_t num_stall_reasons = cupti_context_map_entry_num_stall_reasons_get(entry);
  uint32_t *stall_reason_index = cupti_context_map_entry_stall_reason_index_get(entry);
  char **stall_reason_names = cupti_context_map_entry_stall_reason_names_get(entry);

  gpu_activity_t gpu_activity;
  memset(&gpu_activity, 0, sizeof(gpu_activity_t));

  if (buffer_pc != NULL && (buffer_pc->totalNumPcs > 0 || buffer_pc->remainingNumPcs > 0)) {
    cupti_pc_sampling_data_t *pc_sampling_data = cupti_pc_sampling_data_produce(
      HPCRUN_CUPTI_ACTIVITY_BUFFER_PC_NUM, num_stall_reasons);
    CUpti_PCSamplingData *user_buffer_pc = cupti_pc_sampling_buffer_pc_get(pc_sampling_data);

    // Need to copy back buffer now, otherwise buffer_pc is not big enough to hold all samples
    CUpti_PCSamplingGetDataParams params = {
      .size = CUpti_PCSamplingGetDataParamsSize,
      .ctx = context,
      .pcSamplingData = user_buffer_pc,
      .pPriv = NULL
    };

    HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingGetData, (&params));
    pc_sampling_activity_set(&gpu_activity, range_id, context_id, cct_node, pc_sampling_data);
    if (cupti_range_mode_get() == CUPTI_RANGE_MODE_SERIAL ||
      (cupti_range_mode_get() == CUPTI_RANGE_MODE_EVEN && cupti_range_interval_get() == GPU_RANGE_DEFAULT_RANGE)) {
      gpu_pc_sampling_info2_process(&gpu_activity);
    } else {
      gpu_operation_multiplexer_push(NULL, NULL, &gpu_activity);
    }

    if (DEBUG) {
      pc_sampling_data_debug(user_buffer_pc, num_stall_reasons);
    }

    while ((user_buffer_pc->totalNumPcs > 0 || user_buffer_pc->remainingNumPcs > 0)) {
      pc_sampling_data = cupti_pc_sampling_data_produce(
        HPCRUN_CUPTI_ACTIVITY_USER_BUFFER_PC_NUM, num_stall_reasons);
      user_buffer_pc = cupti_pc_sampling_buffer_pc_get(pc_sampling_data);

      params.pcSamplingData = user_buffer_pc;

      HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingGetData, (&params));
      pc_sampling_activity_set(&gpu_activity, range_id, context_id, cct_node, pc_sampling_data);
      if (cupti_range_mode_get() == CUPTI_RANGE_MODE_SERIAL ||
        (cupti_range_mode_get() == CUPTI_RANGE_MODE_EVEN && cupti_range_interval_get() == GPU_RANGE_DEFAULT_RANGE)) {
        gpu_pc_sampling_info2_process(&gpu_activity);
      } else {
        gpu_operation_multiplexer_push(NULL, NULL, &gpu_activity);
      }

      if (DEBUG) {
        pc_sampling_data_debug(user_buffer_pc, num_stall_reasons);
      }
    }
  }
}

//******************************************************************************
// interface operations
//******************************************************************************


void
cupti_pc_sampling_config
(
 CUcontext context,
 int frequency
)
{
  uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  TMSG(CUPTI, "PC sampling config context_id %u frequency %d", context_id, frequency);

  cupti_context_map_init(context);

  CUpti_PCSamplingConfigurationInfo *config_info = (CUpti_PCSamplingConfigurationInfo *)
    calloc(CONFIG_INFO_COUNT, sizeof(CUpti_PCSamplingConfigurationInfo));

  CUpti_PCSamplingConfigurationInfo *collection_mode_info = &config_info[COLLECTION_MODE];
  CUpti_PCSamplingConfigurationInfo *sampling_period_info = &config_info[SAMPLING_PERIOD];
  CUpti_PCSamplingConfigurationInfo *stall_reason_info = &config_info[STALL_REASONS];
  CUpti_PCSamplingConfigurationInfo *sampling_buffer_info = &config_info[SAMPLING_BUFFER];
  CUpti_PCSamplingConfigurationInfo *scratch_buffer_size_info = &config_info[SCRATCH_BUFFER_SIZE];
  CUpti_PCSamplingConfigurationInfo *hw_buffer_size_info = &config_info[HARDWARE_BUFFER_SIZE];
  CUpti_PCSamplingConfigurationInfo *start_stop_control_info = &config_info[STOP_CONTROL];

  sampling_period_config(frequency, sampling_period_info);

  stall_reason_config(context, stall_reason_info);

  sampling_buffer_config(context, sampling_buffer_info);

  scratch_buffer_size_config(HPCRUN_CUPTI_ACTIVITY_SW_BUFFER_SIZE, scratch_buffer_size_info);

  hardware_buffer_size_config(HPCRUN_CUPTI_ACTIVITY_HW_BUFFER_SIZE, hw_buffer_size_info);

  // we collect pc samples at the end of each range and attribute pc samples postmortem.
  // This mode requires user controlled pc sampling start/stop support and only
  // works for apps with a single cuda context due to the limitation of CUPTI (<=11.6).
  if (cupti_range_mode_get() == CUPTI_RANGE_MODE_SERIAL) {
    // PC sampling is always active
    pc_sampling_active = true;
    collection_mode_config(CUPTI_PC_SAMPLING_COLLECTION_MODE_KERNEL_SERIALIZED, collection_mode_info);
    start_stop_control_config(0, start_stop_control_info);
  } else {
    collection_mode_config(CUPTI_PC_SAMPLING_COLLECTION_MODE_CONTINUOUS, collection_mode_info);
    start_stop_control_config(1, start_stop_control_info);
  }

  CUpti_PCSamplingConfigurationInfoParams info_params = {
    .size = CUpti_PCSamplingConfigurationInfoParamsSize,
    .pPriv = NULL,
    .ctx = context,
    .numAttributes = CONFIG_INFO_COUNT,
    .pPCSamplingConfigurationInfo = config_info
  };

  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingSetConfigurationAttribute, (&info_params));
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


uint64_t
cupti_cubin_crc_get
(
 const void *cubin,
 uint32_t cubin_size
)
{
  CUpti_GetCubinCrcParams params = {
    .size = CUpti_GetCubinCrcParamsSize,
    .cubinSize = cubin_size,
    .cubin = cubin
  };

  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiGetCubinCrc, (&params));

  return params.cubinCrc;
}


void
cupti_pc_sampling_enable2
(
 CUcontext context
)
{
  uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  TMSG(CUPTI, "PC sampling enable context_id %u", context_id);

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
  uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  TMSG(CUPTI, "PC sampling disable context_id %u", context_id);

  CUpti_PCSamplingDisableParams params = {
    .size = CUpti_PCSamplingDisableParamsSize,
    .ctx = context,
    .pPriv = NULL
  };
  HPCRUN_CUPTI_PC_SAMPLING_CALL(cuptiPCSamplingDisable, (&params));
}


void
cupti_pc_sampling_free
(
 CUcontext context
)
{
  uint32_t context_id = ((hpctoolkit_cuctx_st_t *)context)->context_id;
  TMSG(CUPTI, "PC sampling free context_id %u", context_id);

  cupti_context_map_entry_t *entry = cupti_context_map_lookup(context);

  if (!entry) {
    return;
  }

  cupti_pc_sampling_data_t *pc_sampling_data = cupti_context_map_entry_pc_sampling_data_get(entry);
  size_t num_stall_reasons = cupti_context_map_entry_num_stall_reasons_get(entry);
  uint32_t *stall_reason_index = cupti_context_map_entry_stall_reason_index_get(entry);
  char **stall_reason_names = cupti_context_map_entry_stall_reason_names_get(entry);

  // Push it to a cstack for reuse
  cupti_pc_sampling_data_free(pc_sampling_data);
  // Data is calloced
  free(stall_reason_index);
  for (size_t i = 0; i < num_stall_reasons; ++i) {
    free(stall_reason_names[i]);
  }
  free(stall_reason_names);

  // Safe to delete this entry now
  cupti_context_map_delete(context);
}


void
cupti_pc_sampling_correlation_context_collect
(
 cct_node_t *cct_node,
 CUcontext context
)
{
  // In the correlation mode, we serialize every kernel.
  // So we don't have to explicitly call pc_sampling_stop.
  pc_sampling_collect(0, cct_node, context);
}


void
cupti_pc_sampling_range_context_collect
(
 uint32_t range_id,
 CUcontext context
)
{
  if (cupti_pc_sampling_active()) {
    cupti_pc_sampling_stop(context);
  }

  pc_sampling_collect(range_id, NULL, context);
}


// A callback function for pc sampling context map
static void
pc_sampling_context_collect
(
 CUcontext context,
 void *args
)
{
  uint32_t range_id = *(uint32_t *)args;
  cupti_pc_sampling_range_context_collect(range_id, context);
}


// Flush every context in this range, if any
void
cupti_pc_sampling_range_collect
(
 uint32_t range_id
)
{
  cupti_context_map_process(pc_sampling_context_collect, (void *)&range_id);
}


bool
cupti_pc_sampling_active
(
)
{
  return pc_sampling_active;
}
