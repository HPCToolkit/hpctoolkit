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

//***************************************************************************
//
// File:
//   Structure-Cache.hpp
//
// Purpose:
//   functions that support management of a cache for hpcstruct files
//
//***************************************************************************

// Cache structure:
//    The top-level directory of the cache is specified by the user.  We refer
//    to it as "CACHE" below, although the user can pick an arbitrary name.
//
//  All binaries in the cache are named by their elf-hash, a cryptographic hash
//    of the file's contents, expressed as a 32-character string.
//      (See .../src/lib/prof-lean for the implementation).
//
//  There are two subdirectories in the CACHE:  CACHE/FLAT and CACHE/PATH
//    CACHE/FLAT is a directory with one entry for every binary stored in the cache
//      Each of those entries is a symlink to a corresponding directory in CACHE/PATH.
//      Each is named by the hash of the binary.
//
//    Entries in CACHE/PATH are stored in subdirectories named with the full path
//      to the binary to which is appended a subdirectory named by the hash.
//      That subdirectory contains a file named "hpcstruct" which is the fully
//      processed structure file for the binary.
//      The gaps file, when generated with "--show-gaps", will be named gaps, in
//      the same subdirectory
//
//    GPU binaries are handled slightly differently.  The name of the GPU binary
//      The structure file when generated with "--gpucfg no", the default, will be named hpcstruct
//      The structure file when generated with "--gpucfg yes" will be named hpcstruct+gpucfg
//
//    If a GPU binary is reprocessed with a different --gpucfg value, there may be
//      three different files in that subdirectory: hpcstruct, hpcstruct+gpucfg, and gaps
//

#ifndef Structure_Cache_hpp
#define Structure_Cache_hpp

#include "Args.hpp"


//***************************************************************************
// interface operations
//***************************************************************************

// Initial routine to open the cache directory
//    Returns the path to the cache directory, if any
//    Returns NULL if no cache directory
//    Also writes a message giving the path to a newly-created cache,
//      or the path of an opened cache, or writes advice to use a cache.
//
char *
setup_cache_dir
(
 const char *cache_dir,
 Args *args
);


//  construct the directory in CACHE/PATH for the file
//    As a side-effect, this routine removes any previous entry
//    in the directory for that path but with a different hash
//
char *
hpcstruct_cache_path_directory
(
 const char *cache_dir,
 const char *binary_abspath,
 const char *hash, // hash for elf file
 const char *suffix  // appended to the hpcstruct file for runs with --gpucfg yes
);


char *
hpcstruct_cache_path_link
(
 const char *binary_abspath,
 const char *hash // hash for elf file
);


char *
hpcstruct_cache_entry
(
 const char *directory,
 const char *kind
);


// Ensure the the cache FLAT subdirectory is created and writeable
//  Returns the absolute path for the entry
//    Will be a file name of the form <cache_dir>/FLAT/<hash>

char *
hpcstruct_cache_flat_entry
(
 const char *cache_dir,
 const char *hash  // hash for elf file
);


bool
hpcstruct_cache_find
(
 const char *cached_entry
);


char *
hpcstruct_cache_hash
(
 const char *binary_abspath
);


bool
hpcstruct_cache_writable
(
 const char *cache_dir
);

#endif
