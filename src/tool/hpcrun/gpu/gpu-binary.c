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
// Copyright ((c)) 2002-2024, Rice University
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

#define _GNU_SOURCE

#include <stdio.h>
#include <libelf.h>
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


#include "../files.h"
#include "../messages/messages.h"
#include "../loadmap.h"
#include "../../../lib/prof-lean/crypto-hash.h"
#include "../../../lib/prof-lean/gpu-binary-naming.h"
#include "../../../lib/prof-lean/spinlock.h"

#include "gpu-binary.h"

//******************************************************************************
// static data
//******************************************************************************

static const char elf_magic_string[] = ELFMAG;
static const uint32_t *elf_magic = (uint32_t *) elf_magic_string;

// magic number for an Intel 'Patch Token' binary. this constant comes from
// Intel's igc/ocl_igc_shared/executable_format/patch_list.h, which can't be
// imported here because the file is C++. good grief.
static const uint32_t MAGIC_INTEL_PATCH_TOKEN = 0x494E5443; // 'I', 'N', 'T', 'C'

static spinlock_t binary_store_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

gpu_binary_kind_t
gpu_binary_kind
(
 const char *mem_ptr,
 size_t mem_size
)
{
  if (mem_size == 0) return gpu_binary_kind_empty;

  if (mem_ptr != 0 && mem_size > sizeof(uint32_t)) {
    uint32_t *magic = (uint32_t *) mem_ptr;

    // Is this an Intel 'Patch Token' binary?
    if (*magic == MAGIC_INTEL_PATCH_TOKEN) return gpu_binary_kind_intel_patch_token;

    // Is this an ELF binary?
    if (*magic == *elf_magic) return gpu_binary_kind_elf;

    return gpu_binary_kind_unknown;
  }
  return gpu_binary_kind_malformed;
}


bool
gpu_binary_validate_magic
(
 const char *mem_ptr,
 size_t mem_size
)
{
  return gpu_binary_kind(mem_ptr, mem_size) != gpu_binary_kind_unknown;
}


bool
gpu_binary_validate
(
 const char *mem_ptr,
 size_t mem_size
)
{
  if (!gpu_binary_validate_magic(mem_ptr, mem_size)) return false;

  // discard const keyword, but don't change the in-memory copy!
  Elf *elf = elf_memory((char *) mem_ptr, mem_size);
  bool retval = false;
  if (elf) {
    GElf_Ehdr header;
    GElf_Ehdr *ehdr = gelf_getehdr (elf, &header);
    if (ehdr) {
      // at present, don't record AMD GPU binaries
      retval = (ehdr->e_machine != EM_AMDGPU);
    }
    elf_end(elf);
  }

  return retval;
}



//******************************************************************************
// interface operations
//******************************************************************************

bool
gpu_binary_store
(
  const char *file_name,
  const void *binary,
  size_t binary_size
)
{
  // Write a file if does not exist
  bool result;
  int fd;
  errno = 0;

  spinlock_lock(&binary_store_lock);

  fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (errno == EEXIST) {
    close(fd);
    result = true;
  } else if (fd >= 0) {
    // Success
    if (write(fd, binary, binary_size) != binary_size) {
      close(fd);
      result = false;
    } else {
      close(fd);
      result = true;
    }
  } else {
    // Failure to open is a fatal error.
    hpcrun_abort("hpctoolkit: unable to open file: '%s'", file_name);
    result = false;
  }

  spinlock_unlock(&binary_store_lock);

  return result;
}

void
gpu_binary_path_generate
(
  const char *file_name,
  char *path
)
{
  size_t used = 0;
  used += sprintf(&path[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&path[used], "%s", "/" GPU_BINARY_DIRECTORY "/");
  mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  used += sprintf(&path[used], "%s", file_name);
  used += sprintf(&path[used], "%s", GPU_BINARY_SUFFIX);
}


uint32_t
gpu_binary_loadmap_insert
(
  const char *device_file,
  bool mark_used
)
{
  uint32_t loadmap_module_id;
  load_module_t *module = NULL;

  hpcrun_loadmap_lock();
  if ((module = hpcrun_loadmap_findByName(device_file)) == NULL) {
    loadmap_module_id = hpcrun_loadModule_add(device_file);
    module = hpcrun_loadmap_findById(loadmap_module_id);
  } else {
    loadmap_module_id = module->id;
  }
  if (mark_used) {
    hpcrun_loadModule_flags_set(module, LOADMAP_ENTRY_ANALYZE);
  }
  hpcrun_loadmap_unlock();

  return loadmap_module_id;
}


bool
gpu_binary_save
(
 const char *mem_ptr,
 size_t mem_size,
 bool mark_used,
 uint32_t *loadmap_module_id
)
{
  if (!gpu_binary_validate(mem_ptr, mem_size)) return false;

  // Generate a hash for the binary
  char hash_buf[CRYPTO_HASH_STRING_LENGTH];
  crypto_compute_hash_string(mem_ptr, mem_size, hash_buf,
    CRYPTO_HASH_STRING_LENGTH);

  // Prepare to a file path to write down the binary
  char device_file[PATH_MAX];
  gpu_binary_path_generate(hash_buf, device_file);

  // Write down the binary and free the space
  bool written = gpu_binary_store(device_file, mem_ptr, mem_size);

  if (written) {
    *loadmap_module_id = gpu_binary_loadmap_insert(device_file, mark_used);
  }

  return written;
}
