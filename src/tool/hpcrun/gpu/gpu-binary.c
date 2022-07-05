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
// Copyright ((c)) 2002-2022, Rice University
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

#include <stdio.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>  // PATH_MAX

//******************************************************************************
// local includes
//******************************************************************************

#include <include/gpu-binary.h>
#include <hpcrun/files.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/loadmap.h>
#include <lib/prof-lean/crypto-hash.h>
#include <lib/prof-lean/spinlock.h>



//******************************************************************************
// static data
//******************************************************************************

static spinlock_t binary_store_lock = SPINLOCK_UNLOCKED;



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

size_t
gpu_binary_compute_hash_string
(
 const char *mem_ptr,
 size_t mem_size,
 char *name
)
{
  // Compute hash for mem_ptr with mem_size
  unsigned char hash[HASH_LENGTH];
  crypto_hash_compute((const unsigned char *)mem_ptr, mem_size, hash, HASH_LENGTH);

  size_t i;
  size_t used = 0;
  for (i = 0; i < HASH_LENGTH; ++i) {
    used += sprintf(&name[used], "%02x", hash[i]);
  }
  return used;
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
  // Generate a hash for the binary
  char hash_buf[HASH_LENGTH * 2];
  gpu_binary_compute_hash_string(mem_ptr, mem_size, hash_buf);

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
