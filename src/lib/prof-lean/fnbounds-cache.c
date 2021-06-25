// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2019, Rice University
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

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "crypto-hash.h"
#include "fnbounds-cache.h"
#include "file-permissions.h"
#include "tool/hpcfnbounds2/server.h"
#include <errno.h>



//******************************************************************************
// system includes
//******************************************************************************

#define UNIT_TEST 0

// convert a hex digit into ASCII
#define HEX_ASCII(digit) ((digit) + (((digit) < 10) ? '0' : 'a'))



//******************************************************************************
// type declarations
//******************************************************************************


//----------------------------------------------------------------------
// type: map_info_t
//
// description:
//   structure to represent a memory-mapped load module. besides the 
//   map address, it includes the length and fd to support unmapping.
//----------------------------------------------------------------------
typedef struct map_info_t {
  int fd;
  void *addr;
  size_t length;
} map_info_t;



//******************************************************************************
// private operations
//******************************************************************************

//----------------------------------------------------------------------
// function: perm_select
//
// description:
//   helper function for assembling requested file mode at runtime
//----------------------------------------------------------------------
static mode_t 
perm_select
(
 int perm,
 mode_t read,
 mode_t write,
 mode_t execute
)
{
  mode_t retval = 0;

  if (perm & PERM_R) retval |= read;
  if (perm & PERM_W) retval |= write;
  if (perm & PERM_X) retval |= execute;

  return retval;
}


//----------------------------------------------------------------------
// function: fnbounds_loadmodule_realpath
//
// description:
//   compute the full path to a loadmodule, eliminating any relative 
//   components 
//
// return values:
//   absolute pathname 
//
// note: 
//   value returned is static. it must be consumed before calling
//   this function again.
//----------------------------------------------------------------------
static const char *
fnbounds_loadmodule_realpath
(
 const char *loadmodule_path
)
{
  static char loadmodule_realpath[PATH_MAX];

  return realpath(loadmodule_path, loadmodule_realpath);
}


//----------------------------------------------------------------------
// function: fnbounds_cache_pathname
//
// description:
//   look up the name of the fnbounds cache directory from the 
//   environment. if not in the environment, return 0
//
// note: return value memoized
//----------------------------------------------------------------------
const char *
fnbounds_cache_pathname
(
 void
)
{
  static int initialized = 0;
  static char *fnbounds_cache_dir = 0;

  if (!initialized) {
    fnbounds_cache_dir = getenv("HPCFNBOUNDS_CACHE_DIR");
    initialized = 1;
  }

  return fnbounds_cache_dir;
}


//----------------------------------------------------------------------
// function: file_map
//
// description:
//   open, determine the size, and mmap the specified file name.
//   any error causes cleanup and failure.
//
// return values:
//   non-null map_info_t* on success; null map_info_t* on failure 
//----------------------------------------------------------------------
static map_info_t *
file_map
(
 const char *pathname
)
{
  static map_info_t mapping;

  struct stat sb;

  mapping.fd = open(pathname, O_RDONLY);

  if (mapping.fd == -1) return 0;

  if (fstat(mapping.fd, &sb) == -1) {
    close(mapping.fd);
    return 0;
  }

  mapping.length = sb.st_size;

  mapping.addr = mmap(NULL, mapping.length, PROT_READ,
                       MAP_PRIVATE, mapping.fd, 0);

  if (mapping.addr == MAP_FAILED) {
    close(mapping.fd);
    return 0;
  }

  return &mapping;
}


//----------------------------------------------------------------------
// function: file_unmap
//
// description:
//   unmap and close the file described by map_info
//
// return values:
//   1 on success; 0 on failure
//----------------------------------------------------------------------
static int
file_unmap
(
 map_info_t *map_info
)
{
  int success = 0;

  if (map_info) {
    success = 1;

    success &= (munmap(map_info->addr, map_info->length) == 0);

    success &= (close(map_info->fd) == 0);
  }

  return success;
}


//----------------------------------------------------------------------
// function: char_to_hex
//
// description:
//   read nbytes of input and write each byte as a pair of hex
//   characters in output.
//
// return values: 
//   none
//----------------------------------------------------------------------
static void
char_to_hex
(
 char *output,
 unsigned char *input,
 int nbytes
)
{
  int i,j;
  for (i = 0, j = 0; i < nbytes; i++) {
    output[j++] = HEX_ASCII(input[i] >> 4);
    output[j++] = HEX_ASCII(input[i] & 0xf);
  }
}


//----------------------------------------------------------------------
// function: fnbounds_cache_loadmodule_filename
//
// description:
//   compute the filename of a cache entry for a loadmodule.
//
// return value:
//   an allocated 64 character name for the load module. the first 32
//   characters are a crypto hash of the realpath to the file. the
//   final 32 characters are a crypto hash of the file contents.
//   caller is owner of the returned string and is responsible for
//   deallocation.
//
// rationale: 
//   different nodes of a cluster may have different versions
//   of a shared library. guard against spurious cache hits by
//   including a has of the file contents. the files are likely to
//   have different timestamps, but be the same size. a hash of the
//   program headers would likely suffice rather than a hash of the
//   whole file, but this is easy.
//
// note: 
//   crypto hash is MD5, which is not high quality, but should be 
//   adequate for this purpose 
//----------------------------------------------------------------------
static char *
fnbounds_cache_loadmodule_filename
(
 const char *path
)
{
  const char *lm_path = fnbounds_loadmodule_realpath(path);
  int hash_length = crypto_hash_length();
  unsigned char hash_string[hash_length * 2];

  memset(hash_string, 0, sizeof(hash_string));
/*
  map_info_t *mapinfo = file_map(lm_path);

  if (mapinfo) {
    crypto_hash_compute(lm_path, strlen(lm_path), hash_string, hash_length);

    crypto_hash_compute(mapinfo->addr, mapinfo->length, 
			&hash_string[hash_length], hash_length);
    file_unmap(mapinfo);
  }
*/

  struct stat stats;

  if(stat(path, &stats) == 0){
    crypto_hash_compute(lm_path, strlen(lm_path), hash_string, hash_length);

    crypto_hash_compute(&stats, sizeof(struct stat), 
			&hash_string[hash_length], hash_length);
  }


  int buffer_length = sizeof(hash_string) * 2; // size in hex is 2x size in bytes
  buffer_length += 1; // space for NULL;

  char *buffer = (char *) malloc(buffer_length);

  char_to_hex(buffer, hash_string, sizeof(hash_string));
  buffer[buffer_length -1] = 0;

  return buffer;
}


//----------------------------------------------------------------------
// function: fnbounds_cache_loadmodule_pathname
//
// description:
//   add the cache directory name to the given filename to compute the
//   complete pathname for a file in the cache.
//
// return value:
//   a static character string of the form 
//   <cache directory>/<loadmodule path>
//----------------------------------------------------------------------
static const char *
fnbounds_cache_loadmodule_pathname
(
 const char *loadmodule_path
)
{
  static char buffer[PATH_MAX+1];
  buffer[0] = 0;

  char *filename = fnbounds_cache_loadmodule_filename(loadmodule_path); 
  const char *dirname = fnbounds_cache_pathname();

  int filename_len = strlen(filename);
  int dirname_len = strlen(dirname);

  assert(filename_len + dirname_len + 1 < PATH_MAX); // space for '/' = 1

  sprintf(buffer, "%s/%s", dirname, filename);

  free(filename);

  return buffer;
}


//----------------------------------------------------------------------
// function: fnbounds_cache_directory_get
//
// description:
//   check if the fnbounds cache is available and has the necessary 
//   permissions. a writer wants the cache readable and writable. a
//   reader only needs it readable.
//
//   if the cache directory does not exist, try to create it.
//
// return values:
//   1 if directory exists and is writable, or can be created as such.
//   0 otherwise.
// 
// note: 
//   return value is memoized to avoid repeating the logic in
//   subsequent invocations.
//----------------------------------------------------------------------
static int
fnbounds_cache_directory_get
(
 int create
)
{
  int is_available = -1; // uninitialized
  struct stat buf;

  if (is_available < 0) { // uninitialized
    const char *dir = fnbounds_cache_pathname();

    if (stat(dir, &buf) == 0) {
      if (S_ISDIR(buf.st_mode)) {
        // directory exists 
        if (file_permissions_file_accessible(&buf, RWX)) {
          // directory is accessible
          is_available = 1; 
        } else {
          is_available = 0;
        }
      } else {
	      is_available = 0;
      }
    } else {
      if (create && mkdir(dir, RWXR_XR_X) == 0) {
	      is_available = 1;
      }
    }
  } 

  return is_available;
}



//******************************************************************************
// interface operations
//******************************************************************************

//----------------------------------------------------------------------
// function: fnbounds_cache_insert
//
// description:
//   for the specified load module, check in the cache for a matching
//   entry. if one is not found, compute and insert one.
//
// return values:
//   1 if the value was computed and inserted.
//   2 if the value was already present.
//   0 value not present and insertion failed.
//----------------------------------------------------------------------
int
fnbounds_cache_insert
(
 const char *loadmodule_path,
 struct fnbounds_create_t *fnbounds
)
{
  int is_available = 0;

  const char *cached_loadmodule_pathname = 
    fnbounds_cache_loadmodule_pathname(loadmodule_path);


  if (file_permissions_contain(cached_loadmodule_pathname, R__)) {
    // file already exists
    is_available = 2;
  } else {
    if (fnbounds_cache_directory_get(RWX)) {
      int success = fnbounds->compute(loadmodule_path, cached_loadmodule_pathname);
      if (success) {
	      is_available = 1; 
      }
    }
  }

  return is_available;
}


//----------------------------------------------------------------------
// function: fnbounds_cache_lookup
//
// description:
//   for the specified load module, check in the cache for a matching
//   entry. 
//
// return values:
//   1 if the value was present.
//   0 value not present.
//----------------------------------------------------------------------
int
fnbounds_cache_lookup
(
 const char *loadmodule_path
)
{
  int is_available = 0;

  const char *cached_loadmodule_pathname = 
    fnbounds_cache_loadmodule_pathname(loadmodule_path);

  if (file_permissions_contain(cached_loadmodule_pathname, R__)) {
    is_available = 1;
  }

  return is_available;
}

//----------------------------------------------------------------------
// function: cached_loadmodule_pathname_get
//
// description:
//   for the specified load module, return the cached pathname.
//   a wrapper for fnbounds_cache_loadmodule_pathname 
//
// return values:
//   <cache directory>/<loadmodule path>
//----------------------------------------------------------------------
char *
cached_loadmodule_pathname_get
(
  char * name
)
{
  return fnbounds_cache_loadmodule_pathname(name);
}


//******************************************************************************
// unit test
//******************************************************************************

#if UNIT_TEST

#include <stdio.h>


int 
fnbounds_create
(
 const char *loadmodule_pathname, 
 const char *cached_loadmodule_pathname
) 
{
  int success = 0;

  int fd = open(cached_loadmodule_pathname, O_CREAT, RW_R__R__);
  FILE *cached_file = fopen(cached_loadmodule_pathname, "w");
  if (cached_file) {
    success = 1;
    int len = strlen(loadmodule_pathname);
    success &= fwrite(loadmodule_pathname, len, 1, cached_file) == 1;
    success &= fclose(cached_file) == 0;
  }

  printf("cache write (%s) of %s\n", success ? "success" : "failure", 
	 cached_loadmodule_pathname); 

  return success;
}

struct fnbounds_create_t fnbounds_creator = { fnbounds_create };


void
test_insert
(
 const char *loadmodule
)
{
  fnbounds_cache_insert(loadmodule, &fnbounds_creator);
}


void
test_loadmodule_path
(
 const char *path
)
{
  printf("loadmodule path = %s\n", path);

  printf("loadmodule realpath = %s\n", fnbounds_loadmodule_realpath(path));

  printf("cached loadmodule filename = %s\n", 
	 fnbounds_cache_loadmodule_filename(path));

  printf("cached loadmodule path = %s\n", 
	 fnbounds_cache_loadmodule_pathname(path));
}

void
listcache
(
 void
)
{
  char buffer[PATH_MAX + 100];
  sprintf(buffer,"/bin/ls -R %s", fnbounds_cache_pathname());
  printf("listing cache directory\n");
  system(buffer);
}


int
main
(
 int argc, 
 char **argv
)
{
  char buffer[PATH_MAX];
  test_loadmodule_path(argv[0]);
  const char *fullpath = realpath(argv[0], buffer);
  test_loadmodule_path(fullpath);
  test_insert(argv[0]);
  test_insert(argv[0]);
  listcache();
}


#endif