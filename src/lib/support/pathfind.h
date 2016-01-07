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
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef pathfind_h
#define pathfind_h

/* pathfind - search for named file in given colon-separated pathlist
 * --------
 * Searches for a file named "name" in each directory in the
 * colon-separated pathlist given as the first argument, and returns
 * the full pathname to the first occurence that has at least the mode
 * bits specified by mode. An empty path in the pathlist is
 * interpreted as the current directory.  Returns NULL if 'name' is
 * not found.
 *
 * The following mode bits are understood:
 *    "r" - read access
 *    "w" - write access
 *    "x" - execute access
 *
 * The returned pointer points to an area that will be reused on subsequent
 * calls to this function, and must not be freed by the caller.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif

extern char*
pathfind(const char* pathList,
	 const char* name,
	 const char* mode);

#ifdef __cplusplus
}
#endif


#endif
