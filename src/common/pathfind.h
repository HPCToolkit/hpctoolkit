// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef pathfind_h
#define pathfind_h

/* pathfind - search for named file in given colon-separated pathlist
 * --------
 * Searches for a file named "name" in each directory in the
 * colon-separated pathlist given as the first argument, and returns
 * the full pathname to the first occurrence that has at least the mode
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
