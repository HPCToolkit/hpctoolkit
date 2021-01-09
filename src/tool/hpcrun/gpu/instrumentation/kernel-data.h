#ifndef gpu_instrumentation_kernel_data_h
#define gpu_instrumentation_kernel_data_h

#include <gtpin.h>

typedef enum {
  KERNEL_DATA_GTPIN
} kernel_data_kind_t;

typedef struct kernel_data_gtpin_inst {
  int32_t offset;
  struct kernel_data_gtpin_inst *next;
} kernel_data_gtpin_inst_t; 

typedef struct kernel_data_gtpin_block {
  int32_t head_offset;
  int32_t tail_offset;
  GTPinMem mem;
  struct kernel_data_gtpin_inst *inst;
  struct kernel_data_gtpin_block *next;
} kernel_data_gtpin_block_t; 

typedef struct kernel_data_gtpin {
  uint64_t kernel_id;
  struct kernel_data_gtpin_block *block;
} kernel_data_gtpin_t; 
  
typedef struct kernel_data {
  uint32_t loadmap_module_id;
  kernel_data_kind_t kind;
  void *data;
} kernel_data_t;

#endif
