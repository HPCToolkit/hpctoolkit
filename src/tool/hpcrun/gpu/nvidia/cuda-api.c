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

//***************************************************************************
//
// File:
//   cuda-api.c
//
// Purpose:
//   wrapper around NVIDIA CUDA layer
//  
//***************************************************************************


//*****************************************************************************
// system include files
//*****************************************************************************

#include <dlfcn.h>
#include <string.h>    // memset

#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <limits.h>    // PATH_MAX
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir

#include <cuda.h>


//*****************************************************************************
// local include files
//*****************************************************************************

#include <lib/prof-lean/spinlock.h>

#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>

#include "cuda-api.h"
#include "cubin-hash-map.h"
#include "cubin-id-map.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define CUDA_API_DEBUG 0

#define CUDA_FN_NAME(f) DYN_FN_NAME(f)

#define CUDA_FN(fn, args) \
  static CUresult (*CUDA_FN_NAME(fn)) args

#if CUDA_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define HPCRUN_CUDA_API_CALL(fn, args) \
{ \
  CUresult error_result = CUDA_FN_NAME(fn) args; \
  if (error_result != CUDA_SUCCESS) { \
    PRINT("cuda api %s returned %d\n", #fn, (int) error_result); \
    exit(-1); \
  } \
}

//******************************************************************************
// static data
//******************************************************************************

static spinlock_t files_lock = SPINLOCK_UNLOCKED;

#ifndef HPCRUN_STATIC_LINK
CUDA_FN
(
 cuDeviceGetAttribute, 
 (
  int* pi, 
  CUdevice_attribute attrib, 
  CUdevice dev
 )
);


CUDA_FN
(
 cuCtxGetCurrent, 
 (
  CUcontext *ctx
 )
);

CUDA_FN
(
 cuCtxGetStreamPriorityRange, 
 (
  int *leastPriority,
  int *greatestPriority
 ) 
);


CUDA_FN
(
  cuStreamCreateWithPriority,
  (
   CUstream *phStream,
   unsigned int flags,
   int priority
  );
);


CUDA_FN
(
  cuStreamSynchronize,
  (
   CUstream hStream
  );
);

#endif

//******************************************************************************
// private operations 
//******************************************************************************

int 
cuda_bind
(
 void
)
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  CHK_DLOPEN(cuda, "libcuda.so", RTLD_NOW | RTLD_GLOBAL);

  CHK_DLSYM(cuda, cuDeviceGetAttribute); 

  CHK_DLSYM(cuda, cuCtxGetCurrent); 

  CHK_DLSYM(cuda, cuCtxGetStreamPriorityRange);

  CHK_DLSYM(cuda, cuStreamCreateWithPriority);

  CHK_DLSYM(cuda, cuStreamSynchronize);

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}


CUstream
cuda_priority_stream_create
(
)
{
  int priority_high, priority_low;
  CUstream stream;
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuCtxGetStreamPriorityRange,
    (&priority_low, &priority_high));
  HPCRUN_CUDA_API_CALL(cuStreamCreateWithPriority,
    (&stream, CU_STREAM_NON_BLOCKING, priority_high));
#endif
  return stream;
}


void
cuda_stream_synchronize
(
 CUstream stream
)
{
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuStreamSynchronize, (stream));
#endif
}


static int 
cuda_device_sm_blocks_query
(
 int major, 
 int minor
)
{
  switch(major) {
  case 7:
  case 6:
    return 32;
  default:
    // TODO(Keren): add more devices
    return 8;
  }
}


static int 
cuda_device_sm_schedulers_query
(
 int major, 
 int minor
)
{
  switch(major) {
  case 7:
    return 4;
  default:
    // TODO(Keren): add more devices
    return 8;
  }
}


//******************************************************************************
// interface operations 
//******************************************************************************

int 
cuda_context
(
 CUcontext *ctx
)
{
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuCtxGetCurrent, (ctx));
  return 0;
#else
  return -1;
#endif
}

int
cuda_device_property_query
(
 int device_id, 
 cuda_device_property_t *property
)
{
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_count, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_clock_rate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_shared_memory, 
     CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_registers, 
     CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_threads, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR, 
     device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->num_threads_per_warp, CU_DEVICE_ATTRIBUTE_WARP_SIZE, 
     device_id));

  int major = 0, minor = 0;

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device_id));

  property->sm_blocks = cuda_device_sm_blocks_query(major, minor);

  property->sm_schedulers = cuda_device_sm_schedulers_query(major, minor);

  return 0;
#else
  return -1;
#endif
}


static bool
cuda_write_cubin
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


void
cuda_load_callback
(
 uint32_t cubin_id, 
 const void *cubin, 
 size_t cubin_size
)
{
  // Compute hash for cubin and store it into a map
  cubin_hash_map_entry_t *entry = cubin_hash_map_lookup(cubin_id);
  unsigned char *hash;
  unsigned int hash_len;
  if (entry == NULL) {
    cubin_hash_map_insert(cubin_id, cubin, cubin_size);
    entry = cubin_hash_map_lookup(cubin_id);
  }
  hash = cubin_hash_map_entry_hash_get(entry, &hash_len);

  // Create file name
  size_t i;
  size_t used = 0;
  char file_name[PATH_MAX];
  used += sprintf(&file_name[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&file_name[used], "%s", "/cubins/");
  mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  for (i = 0; i < hash_len; ++i) {
    used += sprintf(&file_name[used], "%02x", hash[i]);
  }
  used += sprintf(&file_name[used], "%s", ".cubin");
  PRINT("cubin_id %d hash %s\n", cubin_id, file_name);

  // Write a file if does not exist
  bool file_flag;
  spinlock_lock(&files_lock);
  file_flag = cuda_write_cubin(file_name, cubin, cubin_size);
  spinlock_unlock(&files_lock);

  if (file_flag) {
    char device_file[PATH_MAX];
    sprintf(device_file, "%s", file_name);
    uint32_t hpctoolkit_module_id;
    load_module_t *module = NULL;
    hpcrun_loadmap_lock();
    if ((module = hpcrun_loadmap_findByName(device_file)) == NULL) {
      hpctoolkit_module_id = hpcrun_loadModule_add(device_file);
    } else {
      hpctoolkit_module_id = module->id;
    }
    hpcrun_loadmap_unlock();
    PRINT("cubin_id %d -> hpctoolkit_module_id %d\n", cubin_id, hpctoolkit_module_id);
    cubin_id_map_entry_t *entry = cubin_id_map_lookup(cubin_id);
    if (entry == NULL) {
      Elf_SymbolVector *vector = computeCubinFunctionOffsets(cubin, cubin_size);
      cubin_id_map_insert(cubin_id, hpctoolkit_module_id, vector);
    }
  }
}


void
cuda_unload_callback
(
 uint32_t cubin_id
)
{
}
