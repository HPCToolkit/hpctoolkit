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

#ifndef fnbounds_cache_h
#define fnbounds_cache_h



//******************************************************************************
// type declarations
//******************************************************************************

typedef int (*fnbounds_compute_fn_t)
(
 const char *loadmodule_pathname, 
 const char *cached_loadmodule_pathname
);

struct fnbounds_create_t {
  fnbounds_compute_fn_t compute;
};



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
);


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
 const char *cached_loadmodule_path
);


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
);


#endif