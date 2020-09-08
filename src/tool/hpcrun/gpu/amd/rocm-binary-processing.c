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

#include "rocm-debug-api.h"
#include "rocm-binary-processing.h"
#include <hpcrun/files.h>
#include <hpcrun/memory/hpcrun-malloc.h>
#include "lib/prof-lean/elf-helper.h"

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct amd_gpu_binary {
  int size;
  char* buf;
} amd_gpu_binary_t;

typedef struct amd_function_table {
  size_t size;
  char** names;
  uint64_t* addrs;
} amd_function_table_t;

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"

//******************************************************************************
// local variables
//******************************************************************************

amd_gpu_binary_t binary;
amd_function_table_t function_table;
static uint32_t amd_gpu_module_id;

//******************************************************************************
// private operations
//******************************************************************************

// TODO:
// construct_amd_gpu_symbols parses the ELF symbol and extract
// the string names of functions.
//
// We have similar code in different places that iterate over
// the ELF symbol table, and doing slightly different things.
// At least, NVIDIA support code iterates over symbol table
// to relocate functions, and hpcfnbounds code iterates over
// symbol table to find function starts.
// It would be good to refactor these ELF operations into commond
// code.

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

// TODO:
// Eventually, we want to write the URI into our load map rather than copying the binary into a file.
// To handle this long-term goal, the URI parsing would have to be code shared by hpcrun, hpcprof,
// and hpcstruct. We can move function parse_amd_gpu_binary_uri to prof-lean direcotry and refactor
// the function to return something that can help identifies the GPU binary specified by the URI.

static void
parse_amd_gpu_binary_uri
(
  char *uri
)
{
  // File URI example: file:///home/users/coe0173/HIP-Examples/HIP-Examples-Applications/FloydWarshall/FloydWarshall#offset=26589&size=31088
  char* filepath = uri + strlen("file://");
  char* filepath_end = filepath;

  // filepath is seperated by either # or ?
  while (*filepath_end != '#' && *filepath_end != '?')
    ++filepath_end;
  *filepath_end = 0;

  // Get the offset field in the uri
  char* uri_suffix = filepath_end + 1;
  char* index = strstr(uri_suffix, "offset=") + strlen("offset=");
  char* endptr;
  unsigned long long offset = strtoull(index, &endptr, 10);

  // Get the size field in the uri
  index = strstr(uri_suffix, "size=") + strlen("size=");
  unsigned long int size = strtoul(index, &endptr, 10);

  PRINT("Parsing URI, filepath %s\n", filepath);
  PRINT("\toffset %lx, size %lx\n", offset, size);

  char* filename = strrchr(filepath, '/') + 1;

  // Create file name
  char gpu_file_path[PATH_MAX];
  size_t used = 0;
  used += sprintf(&gpu_file_path[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&gpu_file_path[used], "%s", "/amd/");
  mkdir(gpu_file_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  used += sprintf(&gpu_file_path[used], "%s.rocm-gpu", filename);

  // We directly return if we have write down this URI
  int wfd;
  wfd = open(gpu_file_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (wfd < 0) {
    return;
  }

  int rfd = open(filepath, O_RDONLY);
  if (rfd < 0) {
    PRINT("\tcannot open the file specified in the file URI\n");
	return;
  }

  if (lseek(rfd, offset, SEEK_SET) < 0) {
    PRINT("\tcannot seek to the offset\n");
	return;
  }

  binary.size = size;
  binary.buf = (char*)hpcrun_malloc(size);
  ssize_t read_bytes = read(rfd, binary.buf, size);
  if (read_bytes != size) {
    PRINT("\tfail to read file, read %d\n", read_bytes);
    perror(NULL);
	return;
  }
  write(wfd, (const void*)(binary.buf), binary.size);
  close(wfd);

  amd_gpu_module_id = hpcrun_loadModule_add(gpu_file_path);
}

static int
parse_amd_gpu_binary
(
  void
)
{
  rocm_debug_api_init();
  size_t code_object_count;
  rocm_debug_api_query_code_object(&code_object_count);

  char* gpu_binary_uri = NULL;
  for (size_t i = 0; i < code_object_count; ++i) {
    char* uri = rocm_debug_api_query_uri(i);

    // Ignore memory URI as it points to amd's gpu runtime
	if (strncmp(uri, "memory://", strlen("memory://")) == 0) {
        continue;
	}

    // Assume we have only one code object
	if (strncmp(uri, "file://", strlen("file://")) == 0) {
        gpu_binary_uri = uri;
		break;
	}
  }

  if (gpu_binary_uri == NULL) {
    rocm_debug_api_fini();
    return -1;
  }

  parse_amd_gpu_binary_uri(gpu_binary_uri);
  elf_version(EV_CURRENT);
  Elf *elf = elf_memory(binary.buf, binary.size);
  if (elf != 0) {
	  construct_amd_gpu_symbols(elf);
	  elf_end(elf);
  }
  rocm_debug_api_fini();
  return 0;
}

// TODO:
// lookup_amd_function is currently implemented as a linear search.
// We would hope to get help from AMD that roctracer will directly
// tell us the offset of a launched kernel, so that we do not have
// to do name matching.
//
// If we got no help, then we would need to refactor the code to
// to handle large GPU binaries. We will need to use more efficient
// lookup data structure, a splay tree or a trie.

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

//******************************************************************************
// interface operations
//******************************************************************************

ip_normalized_t
rocm_binary_function_lookup
(
  const char* kernel_name
)
{
  // TODO:
  // 1. Handle multi-threaded case. Currently, this function is called when the first
  //    HIP kernel launch is done. So multiple threads can enter this concurrently.
  // 2. Handle the case of multiple GPU binaries. Currently, this code assuems there is
  //    only one AMD GPU binary. We will need to both assign different names to AMD GPU
  //    binaries and also handle the possibility of one kernel name appearing in different
  //    GPU binaries.

  if (binary.size == 0) {
    if (parse_amd_gpu_binary() < 0) {
      binary.size = -1;
    }
  }
  PRINT("binary size %lx\n", binary.size);
  ip_normalized_t nip;
  if (binary.size <= 0) {
    // This can happen when we failed to get URI
    nip.lm_id = 0;
    nip.lm_ip = 0;
    PRINT("Failed to get amd gpu binary\n");
  } else {
    nip.lm_id = amd_gpu_module_id;
    nip.lm_ip = lookup_amd_function(kernel_name);
    PRINT("HIP launch kernel %s, lm_ip %lx\n", kernel_name, nip.lm_ip);
  }
  return nip;
}
