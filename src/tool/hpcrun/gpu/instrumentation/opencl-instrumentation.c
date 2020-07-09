
//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <gtpin.h>
#include <stdlib.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/safe-sampling.h>
#include <hpcrun/cct/cct.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-correlation.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>

#include "opencl-instrumentation.h"



//******************************************************************************
// local data
//******************************************************************************

#define MAX_STR_SIZE 1024

static atomic_long correlation_id;


//******************************************************************************
// private operations
//******************************************************************************

static void
knobAddBool
(
	const char *name,
	bool value
)
{
  GTPinKnob knob = KNOB_FindArg(name);
  assert(knob != NULL);
  KnobValue knob_value;
  knob_value.value._bool = value;
  knob_value.type = KNOB_TYPE_BOOL;
  KNOB_STATUS status = KNOB_AddValue(knob, &knob_value);
  assert(status == KNOB_STATUS_SUCCESS);
}


static uint32_t
getCorrelationId
(
  void
)
{
  return atomic_fetch_add(&correlation_id, 1);
}


static void
createKernelNode
(
	uint64_t correlation_id
)
{
	cct_node_t *api_node = gpu_application_thread_correlation_callback(correlation_id);
	gpu_correlation_id_map_insert(correlation_id, correlation_id);

	gpu_op_ccts_t gpu_op_ccts;
	gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
	gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);

	hpcrun_safe_enter();
	gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
	hpcrun_safe_exit();

	gpu_activity_channel_consume(gpu_metrics_attribute);
	uint64_t cpu_submit_time = hpcrun_nanotime();
  gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
}


static uint32_t
add_opencl_binary_to_loadmap
(
	char *bin_filename
)
{
	uint32_t hpctoolkit_module_id;
	load_module_t *module = NULL;

	hpcrun_loadmap_lock();
	if ((module = hpcrun_loadmap_findByName(bin_filename)) == NULL) {
		hpctoolkit_module_id = hpcrun_loadModule_add(bin_filename);
	} else {
		hpctoolkit_module_id = module->id;
	}
	hpcrun_loadmap_unlock();
	return hpctoolkit_module_id;
}


static uint32_t
save_opencl_binary
(
	GTPinKernel kernel,
	char *bin_name
)
{
	// dump the binary to files for using it at inside hpcprof 
	uint32_t kernel_binary_size = 0;
  GTPINTOOL_STATUS status = GTPin_GetKernelBinary(kernel, 0, NULL, &kernel_binary_size);
  assert(status == GTPINTOOL_STATUS_SUCCESS);

	uint8_t *binary = (uint8_t*) malloc(sizeof(uint8_t) * kernel_binary_size);

	/*!
	 * Copy original kernel's binary into specified buffer
	 * @ingroup KERNEL
	 * @param[in]        kernel         the target kernel.
	 * @param[in]        buffer_size    size of the buffer in bytes.Ignored,
	 *                                  if buffer is not provided('buf' is NULL)
	 * @param[out, opt]  buf            buffer that receives the requested binary code. NULL pointer can be used to
	 *                                  check actual size of the string without copying it into a client's buffer.
	 * @param[out, opt]  binary_size    If specified(not NULL), receives the actual size of the requested binary in
	 *                                  bytes, including terminating NULL.
	 *
	 * @par Availability:
	 * - OnKernelComplete
	 */
  status = GTPin_GetKernelBinary(kernel, kernel_binary_size, (char *)(binary), NULL);

	strcat(bin_name, "_kernel.bin");
	FILE *bin_ptr = fopen(bin_name, "wb");
	fwrite(binary, kernel_binary_size, 1, bin_ptr);
  assert(status == GTPINTOOL_STATUS_SUCCESS);
	return add_opencl_binary_to_loadmap(bin_name);
}


static void
opencl_activity_notify
(
  void
)
{
  gpu_monitoring_thread_activities_ready();
}


static void
opencl_kernel_block_activity_translate
(
	gpu_activity_t *ga,
	uint32_t correlation_id,
	uint32_t loadmap_module_id,
	uint64_t offset,
	uint64_t execution_count
)
{
	memset(&ga->details.kernel_block, 0, sizeof(gpu_kernel_block_t));
	ga->details.kernel_block.correlation_id = correlation_id;
	ga->details.kernel_block.pc.lm_id = (uint16_t)loadmap_module_id;
	ga->details.kernel_block.pc.lm_ip = (uintptr_t)offset;
	ga->details.kernel_block.offset = offset;
	ga->details.kernel_block.execution_count = execution_count;
	ga->kind = GPU_ACTIVITY_KERNEL_BLOCK;
}


static void
opencl_kernel_block_activity_process
(
	gpu_activity_t *ga,
	uint32_t correlation_id,
	uint32_t loadmap_module_id,
	uint64_t offset,
	uint64_t execution_count
)
{
	opencl_kernel_block_activity_translate(ga, correlation_id, loadmap_module_id, offset, execution_count);
	gpu_activity_process(ga);
}


static void
onKernelBuild
(
	GTPinKernel kernel,
	void *v
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;

  assert(kernel_memory_map_lookup1((uint64_t)kernel) == 0);
  assert(kernel_data_map_lookup1((uint64_t)kernel) == 0);
	
  KernelData data;

	uint32_t correlation_id = getCorrelationId();
	data.kernel_cct_correlation_id = correlation_id;
	createKernelNode(correlation_id);

	mem_pair_node *h;
	mem_pair_node *current;
	bool isHeadNull = true;

  for (GTPinBBL block = GTPin_BBLHead(kernel); GTPin_BBLValid(block); block = GTPin_BBLNext(block)) {
    GTPinINS head = GTPin_InsHead(block);
    assert(GTPin_InsValid(head));
    
		/*!
		 * @return the offset of the instruction relative to the beginning of the original kernel's binary
		 * -1 is returned in case of an error
		 * @ingroup INS
		 * @param[in]   ins the instruction handle.
		 *
		 * @par Availability:
		 * - OnKernelBuild
		 */
		int32_t offset =  GTPin_InsOffset(head);

    GTPinMem mem = NULL;
    status = GTPin_MemClaim(kernel, sizeof(uint32_t), &mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);

		/*!
		* Insert instrumentaion (Opcodeprof) that counts the number of dynamic executions of basic block
		* *countSlot++
		*
		* @ingroup INSTRUMENTATION
		*
		* @param[in]       ins         instruction to be instrumented. The instrumentation code will be inserted
		*                              BEFORE this instruction
		*
		* @param[in]       countSlot   memory slot to store the resulting counter in. The slot should be allocate
		*                              by the GTPin_MemClaim() function, prior to this function call
		*
		* @return  Success/failure status
		*
		* @par Availability:
		* - OnKernelBuild
		*/
    status = GTPin_OpcodeprofInstrument(head, mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);

		mem_pair_node *m = malloc(sizeof(mem_pair_node));
		m->offset = offset;
		m->mem = mem;
		m->next = NULL;

		if (isHeadNull == true) {
			h = m;
			current = m;
			isHeadNull = false;
		} else {
			current->next = m;
		}
  }
	if (h != NULL) {
		kernel_memory_map_insert1((uint64_t)kernel, h);
	}

	gpu_activity_channel_consume(gpu_metrics_attribute);

  char kernel_name[MAX_STR_SIZE];
  status = GTPin_KernelGetName(kernel, MAX_STR_SIZE, kernel_name, NULL);
  assert(status == GTPINTOOL_STATUS_SUCCESS);

	// 
	// m->next = NULL;
	// add these details to cct_node. If thats not needed, we can create the kernel_cct in onKernelComplete
  data.name = kernel_name;
  data.call_count = 0;
	data.loadmap_module_id = save_opencl_binary(kernel, kernel_name);
	
	kernel_data_map_insert1((uint64_t)kernel, data);
  ETMSG(OPENCL, "onKernelBuild complete. Inserted key: %"PRIu64 "",(uint64_t)kernel);
}


static void
onKernelRun
(
	GTPinKernelExec kernelExec,
	void *v
)
{
	GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPin_KernelProfilingActive(kernelExec, 1);
  assert(status == GTPINTOOL_STATUS_SUCCESS);
}


static void
onKernelComplete
(
	GTPinKernelExec kernelExec,
	void *v
)
{
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPinKernel kernel = GTPin_KernelExec_GetKernel(kernelExec);
  ETMSG(OPENCL, "onKernelComplete starting. Lookup: key: %"PRIu64 "",(uint64_t)kernel);
  assert(kernel_data_map_lookup1((uint64_t)kernel) != 0);
  assert(kernel_memory_map_lookup1((uint64_t)kernel) != 0);

	kernel_data_map_t *kernel_data_list = kernel_data_map_lookup1((uint64_t)kernel);
  KernelData data = kernel_data_list->data;
	kernel_memory_map_t *kernel_memory_list = kernel_memory_map_lookup1((uint64_t)kernel);
	mem_pair_node *block = kernel_memory_list->head;

 	// get kernel cct root node from correlation_id
	uint32_t correlation_id = data.kernel_cct_correlation_id;

	while (block != NULL) {
		/*!
		 * @return sampling size for mem handle
		 * @ingroup MEM
		 * @param[in]   mem     the memory handle
		 *
		 * @par Availability:
		 * - all callbacks
		 */
    uint32_t thread_count = GTPin_MemSampleLength(block->mem);
    assert(thread_count > 0);

    uint32_t total = 0, value = 0;
    for (uint32_t tid = 0; tid < thread_count; ++tid) {
    	status = GTPin_MemRead(block->mem, tid, sizeof(uint32_t), (char*)(&value), NULL);
    	assert(status == GTPINTOOL_STATUS_SUCCESS);
    	total += value;
    }

    //block_map_t *bm = block_map_lookup1(data.block_map_root, block->offset);
    //assert(bm != 0);
		uint64_t execution_count = total; // + bm->val 
    //block_map_insert1(data.block_map_root, block->offset, execution_count);
	
		opencl_activity_notify();	
		gpu_activity_t gpu_activity;
		opencl_kernel_block_activity_process(&gpu_activity, correlation_id, data.loadmap_module_id, block->offset, execution_count);
		block = block->next;
		//how to make offset the primary key within the cct and += the execution value for existing ccts?
  }

  ++(data.call_count);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_instrumentation_initialize
(
  void
)
{
  atomic_store(&correlation_id, 5000);	// to avoid conflict with opencl operation correlation ids, we start instrumentation ids with 5000 (TODO:FIX)
}


void enableProfiling
(
  void
)
{
  ETMSG(OPENCL, "inside enableProfiling");
	opencl_instrumentation_initialize();
	knobAddBool("silent_warnings", true);

	/*if (utils::GetEnv("PTI_GEN12") != nullptr) {
    std::cout << "[INFO] Experimental GTPin mode: GEN12" << std::endl;
    KnobAddBool("gen12_1", true);
  }*/

  GTPin_OnKernelBuild(onKernelBuild, NULL);
  GTPin_OnKernelRun(onKernelRun, NULL);
  GTPin_OnKernelComplete(onKernelComplete, NULL);

  GTPIN_Start();
}
