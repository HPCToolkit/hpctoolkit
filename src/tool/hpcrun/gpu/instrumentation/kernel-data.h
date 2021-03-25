#ifndef gpu_instrumentation_kernel_data_h
#define gpu_instrumentation_kernel_data_h

//******************************************************************************
// system includes
//******************************************************************************

#include <gtpin.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gtpin-instrumentation.h"



//******************************************************************************
// type definitions
//******************************************************************************

typedef enum {
  KERNEL_DATA_GTPIN
} kernel_data_kind_t;

typedef struct kernel_data_gtpin_inst {
  int32_t offset;
  int execSize;
  float W_ins;
  bool isPredictable;
  bool isComplex;
  uint32_t aggregated_latency;
  struct kernel_data_gtpin_inst *next;
} kernel_data_gtpin_inst_t; 

typedef struct kernel_data_gtpin_block {
  int32_t head_offset;
  int32_t tail_offset;
  bool hasLatencyInstrumentation;
  GTPinMem mem_latency;
  GTPinMem mem_opcode;
  SimdSectionNode *simd_mem_list;
  uint32_t scalar_instructions;
  uint64_t execution_count;
  uint64_t aggregated_latency;
  int instruction_count;
  struct kernel_data_gtpin_inst *inst;
  struct kernel_data_gtpin_block *next;
} kernel_data_gtpin_block_t; 

typedef struct kernel_data_gtpin {
  uint64_t kernel_id;
  uint64_t simd_width;
  struct kernel_data_gtpin_block *block;
} kernel_data_gtpin_t; 
  
typedef struct kernel_data {
  uint32_t loadmap_module_id;
  kernel_data_kind_t kind;
  void *data;
} kernel_data_t;

#endif
