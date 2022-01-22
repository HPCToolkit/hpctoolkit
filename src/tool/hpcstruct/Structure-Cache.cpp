// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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

//***************************************************************************
//
// File:
//   Structure-Cache.cpp
//
// Purpose:
//   functions that support management of a cache for hpcstruct files
//
//***************************************************************************

//***************************************************************************
// global includes
//***************************************************************************

#include <dirent.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


//***************************************************************************
// local includes
//***************************************************************************

#include <lib/prof-lean/elf-hash.h>
#include <lib/support/diagnostics.h>
#include <lib/support/Exception.hpp>
#include <lib/support/FileUtil.hpp>

#include "Structure-Cache.hpp"



//***************************************************************************
// macros
//***************************************************************************

#define STRUCT_CACHE_DEFAULT "structs-cache"
#define STRUCT_CACHE_ENV "HPCTOOLKIT_HPCSTRUCT_CACHE"



//***************************************************************************
// local operations
//***************************************************************************

// return value
//   1: success
//   0: failure
static int
path_accessible
(
 const char *path,
 int access_flags
)
{
  int status = 0;

  int fd = open(path, access_flags | O_CLOEXEC);
  if (fd != -1) {
    status = 1; // found!
    close(fd); // cleanup
  }

  return status;
}


static void
mkpath
(
 const char *path,
 const char *errortype
)
{
  try {
    FileUtil::mkdir(path);
  } catch (Diagnostics::FatalException &e) {
    std::cerr << "ERROR: " << errortype << " - " << e.what() << std::endl;
    throw e;
  }
}


// determine if there already exists a different hash entry for a file path
// 1: cleanup needed
// 0: no cleanup needed
static int
hpcstruct_cache_needs_cleanup
(
 std::string path,
 const char *hash
)
{
  DIR *dir = opendir(path.c_str());

  for (;;) {
    struct dirent *d = readdir(dir);

    if (d == 0) break;

    // ignore "." and ".."
    if ((strcmp(d->d_name, ".") == 0) ||
	(strcmp(d->d_name, "..") == 0)) continue;

    if (strcmp(hash, d->d_name) == 0) { // hash present; nothing to do
      continue;
    } else {
      // something else present: cleanup needed
      closedir(dir);
      return 1;
    }
  }

  closedir(dir);

  return 0;
}


static int
hpcstruct_cache_cleanup
(
 std::string path,
 const char *hash
)
{
  int cleanup = hpcstruct_cache_needs_cleanup(path, hash);

  if (cleanup) {
    std::string command("rm -rf ");
    command += path;
    system(command.c_str());
    mkpath(path.c_str(), "Failed to clean up hpcstruct cache directory");
  }

  return 0;
}


static bool
empty_string
(
 const char *s
)
{
  return s == 0 || *s == 0;
}



//***************************************************************************
// interface operations
//***************************************************************************

bool
hpcstruct_cache_find
(
 const char *cached_entry
)
{
  return path_accessible(cached_entry, O_RDONLY);
}


bool
hpcstruct_cache_writable
(
 const char *cache_dir
)
{
  return access(cache_dir, O_RDWR) == 0;
}


char *
hpcstruct_cache_hash
(
 const char *binary_abspath
)
{
  char *eh  = elf_hash(binary_abspath);
  return eh;
}


char *
hpcstruct_cache_flat_directory
(
 const char *cache_dir,
 const char *hash // hash for elf file
)
{
  std::string path = cache_dir;

  path += "/FLAT";

  mkpath(path.c_str(), "Failed to create entry in hpcstruct cache directory");

  // compute the full path to the new cache directory
  path = path + '/' + hash;

  // return the full path for the new cache entry
  return strdup(path.c_str());
}


char *
hpcstruct_cache_path_directory
(
 const char *cache_dir,
 const char *binary_abspath,
 const char *hash // hash for elf file
)
{
  std::string path = cache_dir;

  path += "/PATH";

  path += binary_abspath;

  // FIXME: catch error
  mkpath(path.c_str(), "Failed to create entry in hpcstruct cache directory");

  // discard any prior entries for path with a different hash
  hpcstruct_cache_cleanup(path.c_str(), hash);

  // compute the full path to the new cache directory
  path = path + '/' + hash;

  // ensure the new cache directory exists
  mkpath(path.c_str(), "Failed to create hpcstruct cache entry");

  // return the full path for the new cache entry
  return strdup(path.c_str());
}


char *
hpcstruct_cache_path_link
(
 const char *binary_abspath,
 const char *hash // hash for elf file
)
{
  std::string path = "../PATH";

  path += binary_abspath;
  path = path + '/' + hash;

  return strdup(path.c_str());
}


char *
hpcstruct_cache_entry
(
 const char *directory,
 const char *kind
)
{
  std::string path = directory;

  // compute the full path for the new cache entry
  path = path + '/' + kind;

  // return the full path for the new cache entry
  return strdup(path.c_str());
}


char *
hpcstruct_cache_directory
(
 const char *cache_dir,
 const char *kind
)
{
  static bool warn = true;
  char abspath[PATH_MAX];

  if (empty_string(cache_dir)) {
    // cache directory is not explicit: consider environment variable value
    cache_dir = getenv(STRUCT_CACHE_ENV);
    if (empty_string(cache_dir)) cache_dir = 0;
  }
  // invariant: if cache_dir was NULL or empty, it is now NULL

  if (cache_dir == 0) {
    // no cache directory specified,
    if (warn) {
      DIAG_MsgIf_GENERIC
	("ADVICE: ", warn, "See the usage message for how to use "
	 "a structure cache to accelerate analysis of CPU and GPU binaries");
      warn = false;
    }
    return 0;
  } else {
    realpath(cache_dir, abspath);
  }

  strcat(abspath, kind);

  mkpath(abspath, "Failed to create hpcstruct cache directory");

  return strdup(abspath);
}


void
hpcstruct_reify_path
(
 const char *path
)
{
  mkpath(path, "Failed to create hpcstruct output directory");
}


void
hpcstruct_reify_path_parent
(
 const char *path
)
{
  std::string outpath = path;
  size_t pos = outpath.find_last_of("/");
  if (pos != std::string::npos) {
    hpcstruct_reify_path(outpath.substr(0, pos).c_str());
  }
}
