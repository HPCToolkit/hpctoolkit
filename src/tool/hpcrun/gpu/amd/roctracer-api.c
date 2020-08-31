// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//******************************************************************************
// system includes
//******************************************************************************

#include <gelf.h>

#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>  // PATH_MAX

//******************************************************************************
// local includes
//******************************************************************************

#include "roctracer-api.h"
#include "roctracer-activity-translate.h"

#include <roctracer_hip.h>

#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample-sources/libdl.h>

#include <hpcrun/utilities/hpcrun-nanotime.h>
#include <hpcrun/files.h>
#include "lib/prof-lean/elf-helper.h"


//******************************************************************************
// macros
//******************************************************************************

#define FORALL_ROCTRACER_ROUTINES(macro)			\
  macro(roctracer_open_pool_expl)   \
  macro(roctracer_flush_activity_expl)   \
  macro(roctracer_activity_push_external_correlation_id) \
  macro(roctracer_activity_pop_external_correlation_id) \
  macro(roctracer_enable_domain_callback) \
  macro(roctracer_enable_domain_activity_expl) \
  macro(roctracer_disable_domain_callback) \
  macro(roctracer_disable_domain_activity) \
  macro(roctracer_set_properties)


#define ROCTRACER_FN_NAME(f) DYN_FN_NAME(f)

#define ROCTRACER_FN(fn, args) \
  static roctracer_status_t (*ROCTRACER_FN_NAME(fn)) args

#define HPCRUN_ROCTRACER_CALL(fn, args) \
{      \
  roctracer_status_t status = ROCTRACER_FN_NAME(fn) args;	\
  if (status != ROCTRACER_STATUS_SUCCESS) {		\
    /* use roctracer_error_string() */ \
  }						\
}

typedef const char* (*hip_kernel_name_fnt)(const hipFunction_t f);
static hip_kernel_name_fnt hip_kernel_name_fn;

typedef const char* (*hip_kernel_name_ref_fnt)(const void* hostFunction, hipStream_t stream);
static hip_kernel_name_ref_fnt hip_kernel_name_ref_fn;

typedef int (*hiprtc_fnt)(uint64_t, void*);
static hiprtc_fnt hiprtc_get_code;
static hiprtc_fnt hiprtc_get_code_size;

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"

//******************************************************************************
// type declaration
//******************************************************************************

typedef struct amd_kernel_internal {
  uint64_t opaque[3];
  uint64_t amd_program_internal;
} amd_kernel_internal_t;

typedef struct amd_hip_function {
  amd_kernel_internal_t * amd_kernel;
} amd_hip_function_t;

typedef struct amd_gpu_binary {
  size_t size;
  char* buf;
} amd_gpu_binary_t;

typedef struct amd_function_table {
  size_t size;
  char** names;
  uint64_t* addrs;
} amd_function_table_t;


//******************************************************************************
// local variables
//******************************************************************************

amd_gpu_binary_t binary;
amd_function_table_t function_table;
static uint32_t amd_gpu_module_id;

//----------------------------------------------------------
// roctracer function pointers for late binding
//----------------------------------------------------------

ROCTRACER_FN
(
 roctracer_open_pool_expl,
 (
  const roctracer_properties_t*,
  roctracer_pool_t**
 )
);

ROCTRACER_FN
(
 roctracer_flush_activity_expl,
 (
  roctracer_pool_t*
 )
);

ROCTRACER_FN
(
 roctracer_activity_push_external_correlation_id,
 (
  activity_correlation_id_t
 )
);

ROCTRACER_FN
(
 roctracer_activity_pop_external_correlation_id,
 (
  activity_correlation_id_t*
 )
);

ROCTRACER_FN
(
 roctracer_enable_domain_callback,
 (
  activity_domain_t,
  activity_rtapi_callback_t,
  void*
 )
);

ROCTRACER_FN
(
 roctracer_enable_domain_activity_expl,
 (
  activity_domain_t,
  roctracer_pool_t*
 )
);

ROCTRACER_FN
(
 roctracer_disable_domain_callback,
 (
  activity_domain_t
 )
);

ROCTRACER_FN
(
 roctracer_disable_domain_activity,
 (
  activity_domain_t
 )
);

ROCTRACER_FN
(
 roctracer_set_properties,
 (
  activity_domain_t,
  void*
 )
);


//******************************************************************************
// private operations
//******************************************************************************

#if 0
static void
roctracer_correlation_id_push
(
 uint64_t id
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_activity_push_external_correlation_id, (id));
}


static void
roctracer_correlation_id_pop
(
 uint64_t* id
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_activity_pop_external_correlation_id, (id));
}
#endif


#if 0
static void
roctracer_kernel_data_set
(
 const hip_api_data_t *data,
 entry_data_t *entry_data,
 uint32_t callback_id
)
{
  switch(callback_id)
    {
    case HIP_API_ID_hipModuleLaunchKernel:
      entry_data->kernel.blockSharedMemory =
	data->args.hipModuleLaunchKernel.sharedMemBytes;

      entry_data->kernel.blockThreads =
	data->args.hipModuleLaunchKernel.blockDimX *
	data->args.hipModuleLaunchKernel.blockDimY *
	data->args.hipModuleLaunchKernel.blockDimZ;
      break;

    case HIP_API_ID_hipLaunchCooperativeKernel:
      entry_data->kernel.blockSharedMemory =
	data->args.hipLaunchCooperativeKernel.sharedMemBytes;

      entry_data->kernel.blockThreads =
	data->args.hipLaunchCooperativeKernel.blockDimX.x *
	data->args.hipLaunchCooperativeKernel.blockDimX.y *
	data->args.hipLaunchCooperativeKernel.blockDimX.z;
      break;

    case HIP_API_ID_hipHccModuleLaunchKernel:
      entry_data->kernel.blockSharedMemory =
	data->args.hipHccModuleLaunchKernel.sharedMemBytes;

      entry_data->kernel.blockThreads =
	(data->args.hipHccModuleLaunchKernel.globalWorkSizeX *
	 data->args.hipHccModuleLaunchKernel.globalWorkSizeY *
	 data->args.hipHccModuleLaunchKernel.globalWorkSizeZ) +
	(data->args.hipHccModuleLaunchKernel.localWorkSizeX *
	 data->args.hipHccModuleLaunchKernel.localWorkSizeY *
	 data->args.hipHccModuleLaunchKernel.localWorkSizeZ);
      break;
    }
}
#endif

static void
ensure_kernel_ip_present
(
 cct_node_t *kernel_ph,
 ip_normalized_t kernel_ip
)
{
  // if the phaceholder was previously inserted, it will have a child
  // we only want to insert a child if there isn't one already. if the
  // node contains a child already, then the gpu monitoring thread
  // may be adding children to the splay tree of children. in that case
  // trying to add a child here (which will turn into a lookup of the
  // previously added child, would race with any insertions by the
  // GPU monitoring thread.
  //
  // INVARIANT: avoid a race modifying the splay tree of children by
  // not attempting to insert a child in a worker thread when a child
  // is already present
  if (hpcrun_cct_children(kernel_ph) == NULL) {
    cct_node_t *kernel =
      hpcrun_cct_insert_ip_norm(kernel_ph, kernel_ip);
    hpcrun_cct_retain(kernel);
  }
}

static void
construct_amd_gpu_symbols
(
 Elf *elf
)
{
  // Initialize elf_help_t to handle extended numbering
  elf_helper_t eh;
  elf_helper_initialize(elf, &eh);

  // Get section name section index to find ".strtab"
  size_t shstrndx;
  if (elf_getshdrstrndx(elf, &shstrndx) != 0) return;

  // Find .symtab and .strtab sections
  Elf_Scn *scn = NULL;
  Elf_Scn *symtab_scn = NULL;
  Elf_Scn *strtab_scn = NULL;
  while ((scn = elf_nextscn(elf, scn)) != NULL) {
    GElf_Shdr shdr;
    if (!gelf_getshdr(scn, &shdr)) continue;
    if (shdr.sh_type == SHT_SYMTAB) {
      symtab_scn = scn;
      continue;
    }
    char *name = elf_strptr(elf, shstrndx , shdr.sh_name);
    if (name == NULL) continue;
    if (strcmp(name, ".strtab") == 0) {
      strtab_scn = scn;
    }
	}

  // Get total number of symbols in .symtab
  GElf_Shdr symtab;
  gelf_getshdr(symtab_scn, &symtab);

  int nsymbols = 0;
  if (symtab.sh_entsize > 0) { // avoid divide by 0
    nsymbols = symtab.sh_size / symtab.sh_entsize;
    if (nsymbols <= 0) return;
  } else {
    return;
  }

  Elf_Data *symtab_data = elf_getdata(symtab_scn, NULL);
  if (symtab_data == NULL) return;

  // Get total number of function symbols in .symtab
  size_t nfuncs = 0;
  for (int i = 0; i < nsymbols; i++) {
    GElf_Sym sym;
    GElf_Sym *symp = NULL;
    int section_index;
    symp = elf_helper_get_symbol(&eh, i, &sym, &section_index);
    if (symp) { // symbol properly read
      int symtype = GELF_ST_TYPE(sym.st_info);
      if (sym.st_shndx == SHN_UNDEF) continue;
      if (symtype != STT_FUNC) continue;
      nfuncs++;
    }
  }

  // Get symbol name string table
  Elf_Data *strtab_data = elf_getdata(strtab_scn, NULL);
  if (strtab_data == NULL) return;
  char* symbol_name_buf = (char*)(strtab_data->d_buf);

  // Initialize our function table
  function_table.size = nfuncs;
  function_table.names = (char**)hpcrun_malloc(sizeof(char*) * function_table.size);
  function_table.addrs = (uint64_t*)hpcrun_malloc(sizeof(uint64_t) * function_table.size);
  int index = 0;

  // Put each function symbol into our function table
  for (int i = 0; i < nsymbols; i++) {
    GElf_Sym sym;
    GElf_Sym *symp = NULL;
    int section_index;
    symp = elf_helper_get_symbol(&eh, i, &sym, &section_index);
    if (symp) { // symbol properly read
      int symtype = GELF_ST_TYPE(sym.st_info);
      if (sym.st_shndx == SHN_UNDEF) continue;
      if (symtype != STT_FUNC) continue;
      function_table.names[index] = symbol_name_buf + sym.st_name;
      function_table.addrs[index] = sym.st_value;
      ++index;
    }
  }
#if DEBUG != 0
  printf("Dump AMD GPU functions\n");
  for (size_t i = 0; i < function_table.size; ++i) {
    printf("Function %s, at address %lx\n", function_table.names[i], function_table.addrs[i]);
  }
#endif
}

static bool
cupti_write_cubin
(
 const char *file_name,
 const void *cubin,
 size_t cubin_size
)
{
  int fd;
  errno = 0;
  fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (errno == EEXIST) {
    close(fd);
    return true;
  }
  if (fd >= 0) {
    // Success
    if (write(fd, cubin, cubin_size) != cubin_size) {
      close(fd);
      return false;
    } else {
      close(fd);
      return true;
    }
  } else {
    // Failure to open is a fatal error.
    hpcrun_abort("hpctoolkit: unable to open file: '%s'", file_name);
    return false;
  }
}

static void
parse_amd_gpu_binary
(
  hipFunction_t f
)
{
  amd_hip_function_t* amd_hip_function = (amd_hip_function_t*)((uint64_t)f + 0x88);
  amd_kernel_internal_t* amd_internal = amd_hip_function->amd_kernel;
  uint64_t amd_program = amd_internal->amd_program_internal;
  amd_program += 0x10;

  hiprtc_get_code_size(amd_program, (void*)(&(binary.size)));
  binary.buf = hpcrun_malloc(binary.size);
  hiprtc_get_code(amd_program, (void*)(binary.buf));

  elf_version(EV_CURRENT);
  Elf *elf = elf_memory(binary.buf, binary.size);
  if (elf != 0) {
	  construct_amd_gpu_symbols(elf);
	  elf_end(elf);
  }

  // Create file name
  char file_name[PATH_MAX];
  size_t used = 0;
  used += sprintf(&file_name[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&file_name[used], "%s", "/amd/");
  mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  used += sprintf(&file_name[used], "%s", "amd_gpu_binary");

  cupti_write_cubin(file_name, binary.buf, binary.size);

  amd_gpu_module_id = hpcrun_loadModule_add(file_name);
}

static uintptr_t
lookup_amd_function
(
  const char *kernel_name
)
{
  for (size_t i = 0; i < function_table.size; ++i) {
    if (strcmp(kernel_name, function_table.names[i]) == 0) {
      return (uintptr_t)(function_table.addrs[i]);
    }
  }
  return 0;
}

static ip_normalized_t
hip_function_lookup
(
  hipFunction_t f
)
{
  // TODO: Handle multi-threaded case
  if (binary.size == 0) {
    parse_amd_gpu_binary(f);
  }

  const char * kernel_name = hip_kernel_name_fn(f);

  ip_normalized_t nip;
  nip.lm_id = amd_gpu_module_id;
  nip.lm_ip = lookup_amd_function(kernel_name);

  PRINT("HIP launch kernel %s, lm_ip %lx\n", kernel_name, nip.lm_ip);
  return nip;
}

static hipFunction_t
get_hip_function_t
(
  const void* host_address,
  hipStream_t stream
)
{
  hipFunction_t handle;
  const char* name = hip_kernel_name_ref_fn(host_address, stream);

  asm ( "mov %%rdi, %0"
    : "=r" (handle)
  );

  return handle;
}

static void
roctracer_subscriber_callback
(
 uint32_t domain,
 uint32_t callback_id,
 const void* callback_data,
 void* arg
)
{
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  bool is_valid_op = false;
  bool is_kernel_op = false;
  const hip_api_data_t* data = (const hip_api_data_t*)(callback_data);
  hipFunction_t f = NULL;

  switch (callback_id) {
  case HIP_API_ID_hipMemcpy:
  case HIP_API_ID_hipMemcpyToSymbolAsync:
  case HIP_API_ID_hipMemcpyFromSymbolAsync:
  case HIP_API_ID_hipMemcpyDtoD:
  case HIP_API_ID_hipMemcpy2DToArray:
  case HIP_API_ID_hipMemcpyAsync:
  case HIP_API_ID_hipMemcpyFromSymbol:
  case HIP_API_ID_hipMemcpy3D:
  case HIP_API_ID_hipMemcpyAtoH:
  case HIP_API_ID_hipMemcpyHtoD:
  case HIP_API_ID_hipMemcpyHtoA:
  case HIP_API_ID_hipMemcpy2D:
  case HIP_API_ID_hipMemcpyPeerAsync:
  case HIP_API_ID_hipMemcpyDtoH:
  case HIP_API_ID_hipMemcpyHtoDAsync:
  case HIP_API_ID_hipMemcpyFromArray:
  case HIP_API_ID_hipMemcpy2DAsync:
  case HIP_API_ID_hipMemcpyToArray:
  case HIP_API_ID_hipMemcpyToSymbol:
  case HIP_API_ID_hipMemcpyPeer:
  case HIP_API_ID_hipMemcpyDtoDAsync:
  case HIP_API_ID_hipMemcpyDtoHAsync:
  case HIP_API_ID_hipMemcpyParam2D:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_copy);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipMalloc:
  case HIP_API_ID_hipMallocPitch:
  case HIP_API_ID_hipMalloc3DArray:
  case HIP_API_ID_hipMallocArray:
  case HIP_API_ID_hipHostMalloc:
  case HIP_API_ID_hipMallocManaged:
  case HIP_API_ID_hipMalloc3D:
  case HIP_API_ID_hipExtMallocWithFlags:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_alloc);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipMemset3DAsync:
  case HIP_API_ID_hipMemset2D:
  case HIP_API_ID_hipMemset2DAsync:
  case HIP_API_ID_hipMemset:
  case HIP_API_ID_hipMemsetD8:
  case HIP_API_ID_hipMemset3D:
  case HIP_API_ID_hipMemsetAsync:
  case HIP_API_ID_hipMemsetD32Async:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_memset);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipFree:
  case HIP_API_ID_hipFreeArray:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_delete);
    is_valid_op = true;
    break;

  case HIP_API_ID_hipModuleLaunchKernel:
  case HIP_API_ID_hipLaunchCooperativeKernel:
  case HIP_API_ID_hipHccModuleLaunchKernel: {
    //case HIP_API_ID_hipExtModuleLaunchKernel:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_kernel);
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_trace);
    is_valid_op = true;
    is_kernel_op = true;
    f = data->args.hipModuleLaunchKernel.f;
    break;
  }
  case HIP_API_ID_hipLaunchKernel: {
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_kernel);
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_trace);
    is_valid_op = true;
    is_kernel_op = true;
    f = get_hip_function_t(data->args.hipLaunchKernel.function_address, data->args.hipLaunchKernel.stream);
    break;
  }
  case HIP_API_ID_hipCtxSynchronize:
  case HIP_API_ID_hipStreamSynchronize:
  case HIP_API_ID_hipDeviceSynchronize:
  case HIP_API_ID_hipEventSynchronize:
    gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
				 gpu_placeholder_type_sync);
    is_valid_op = true;
    break;
  case HIP_API_ID_hipModuleGetFunction:
    PRINT("hipModuleGetFunction %s\n", data->args.hipModuleGetFunction.kname);
    break;
  case HIP_API_ID_hipModuleLoad:
    PRINT("hipMOoduleLoad %p\n", data->args.hipModuleLoad.module);
    break;
  default:
    break;
  }

  if (!is_valid_op) return;


  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    uint64_t correlation_id = data->correlation_id;
    cct_node_t *api_node =
      gpu_application_thread_correlation_callback(correlation_id);

    gpu_op_ccts_t gpu_op_ccts;
    hpcrun_safe_enter();
    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
    if (is_kernel_op) {

      ip_normalized_t kernel_ip = hip_function_lookup(f);

      cct_node_t *kernel_ph = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_kernel);
      ensure_kernel_ip_present(kernel_ph, kernel_ip);

      cct_node_t *trace_ph = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_trace);
      ensure_kernel_ip_present(trace_ph, kernel_ip);
    }

    hpcrun_safe_exit();


    gpu_activity_channel_consume(gpu_metrics_attribute);

    // Generate notification entry
    uint64_t cpu_submit_time = hpcrun_nanotime();
    gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
  }
}


static void
roctracer_buffer_completion_notify
(
  void
)
{
  gpu_monitoring_thread_activities_ready();
}


static void
roctracer_activity_process
(
 roctracer_record_t* roctracer_record
)
{
  gpu_activity_t gpu_activity;
  roctracer_activity_translate(&gpu_activity, roctracer_record);
  if (gpu_correlation_id_map_lookup(roctracer_record->correlation_id) == NULL) {
    gpu_correlation_id_map_insert(roctracer_record->correlation_id,
				  roctracer_record->correlation_id);
  }
  gpu_activity_process(&gpu_activity);
}


static void
roctracer_buffer_completion_callback
(
 const char* begin,
 const char* end,
 void* arg
)
{
  roctracer_buffer_completion_notify();
  roctracer_record_t* record = (roctracer_record_t*)(begin);
  roctracer_record_t* end_record = (roctracer_record_t*)(end);
  while (record < end_record) {
    roctracer_activity_process(record);
    record++;
  }
}


static const char *
roctracer_path
(
 void
)
{
  const char *path = "libroctracer64.so";

  return path;
}



//******************************************************************************
// interface operations
//******************************************************************************

int
roctracer_bind
(
 void
)
{
  // This is a workaround for roctracer to not hang when taking timer interrupts
  // More details: https://github.com/ROCm-Developer-Tools/roctracer/issues/22
  setenv("HSA_ENABLE_INTERRUPT", "0", 1);

#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(roctracer, roctracer_path(), RTLD_NOW | RTLD_GLOBAL);
  // Somehow roctracter needs libkfdwrapper64.so, but does not really load it.
  // So, we load it before using any function in roctracter.
  CHK_DLOPEN(kfd, "libkfdwrapper64.so", RTLD_NOW | RTLD_GLOBAL);

  CHK_DLOPEN(hip, "libamdhip64.so", RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define ROCTRACER_BIND(fn) \
  CHK_DLSYM(roctracer, fn);

  FORALL_ROCTRACER_ROUTINES(ROCTRACER_BIND)

#undef ROCTRACER_BIND

  dlerror();
  hip_kernel_name_fn = (hip_kernel_name_fnt) dlsym(hip, "hipKernelNameRef");
  if (hip_kernel_name_fn == 0) {
    return -1;
  }

  dlerror();
  hip_kernel_name_ref_fn = (hip_kernel_name_ref_fnt) dlsym(hip, "hipKernelNameRefByPtr");
  if (hip_kernel_name_ref_fn == 0) {
    return -1;
  }

  dlerror();
  hiprtc_get_code = (hiprtc_fnt) dlsym(hip, "hiprtcGetCode");
  if (hiprtc_get_code == 0) {
    return -1;
  }

  dlerror();
  hiprtc_get_code_size = (hiprtc_fnt) dlsym(hip, "hiprtcGetCodeSize");
  if (hiprtc_get_code_size == 0) {
    return -1;
  }

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}

void
roctracer_init
(
 void
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_set_properties, (ACTIVITY_DOMAIN_HIP_API, NULL));
  // Allocating tracing pool
  roctracer_properties_t properties;
  memset(&properties, 0, sizeof(roctracer_properties_t));
  properties.buffer_size = 0x1000;
  properties.buffer_callback_fun = roctracer_buffer_completion_callback;
  HPCRUN_ROCTRACER_CALL(roctracer_open_pool_expl, (&properties, NULL));
  // Enable HIP API callbacks
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_HIP_API, roctracer_subscriber_callback, NULL));
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_HSA_API, roctracer_subscriber_callback, NULL));
  // Enable HIP activity tracing
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_activity_expl, (ACTIVITY_DOMAIN_HIP_API, NULL));
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_activity_expl, (ACTIVITY_DOMAIN_HCC_OPS, NULL));

  // Enable PC sampling
  //HPCRUN_ROCTRACER_CALL(roctracer_enable_op_activity, (ACTIVITY_DOMAIN_HSA_OPS, HSA_OP_ID_PCSAMPLE));
  // Enable KFD API tracing
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_KFD_API, roctracer_subscriber_callback, NULL));
  // Enable rocTX
  HPCRUN_ROCTRACER_CALL(roctracer_enable_domain_callback, (ACTIVITY_DOMAIN_ROCTX, roctracer_subscriber_callback, NULL));
}

void
roctracer_fini
(
 void* args
)
{
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_callback, (ACTIVITY_DOMAIN_HIP_API));
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_activity, (ACTIVITY_DOMAIN_HIP_API));
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_activity, (ACTIVITY_DOMAIN_HCC_OPS));
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_activity, (ACTIVITY_DOMAIN_HSA_OPS));
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_callback, (ACTIVITY_DOMAIN_KFD_API));
  HPCRUN_ROCTRACER_CALL(roctracer_disable_domain_callback, (ACTIVITY_DOMAIN_ROCTX));
  HPCRUN_ROCTRACER_CALL(roctracer_flush_activity_expl, (NULL));

  gpu_application_thread_process_activities();
}

