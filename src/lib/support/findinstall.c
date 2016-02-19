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

/*************************** System Include Files ***************************/

#include <stdio.h>  // for FILENAME_MAX
#include <libgen.h> // for dirname/basename

#define __USE_XOPEN_EXTENDED // for realpath()
#include <stdlib.h>

#define __USE_XOPEN_EXTENDED // for strdup()
#include <string.h>

/**************************** User Include Files ****************************/

#include "findinstall.h"
#include "pathfind.h"

/*************************** Forward Declarations ***************************/

/****************************************************************************/

/* findinstall
 *  
 * Note: This is a little trickier than a shell script launcher
 * because if cmd was invoked as 'foo' with the assumption that it
 * would be found using PATH, 'cmd' is _not_ a fully resolved a path,
 * but simply 'foo'.
*/
char*
findinstall(const char* cmd, const char* base_cmd)
{
  static char rootdir[FILENAME_MAX]; // PATH_MAX
  char *bindir, *rootdir_rel;
  char *cmd1 = NULL, *bindir_tmp = NULL;

  rootdir[0] = '\0';
  if (!cmd || !base_cmd) {
    return NULL;
  }

  /* given no path information, we must use pathfind */
  if (strcmp(cmd, base_cmd) == 0) {
    cmd1 = pathfind(getenv("PATH"), cmd, "rx");
  }
  if (!cmd1) {
    cmd1 = (char*)cmd;
  }
  
  /* given a command with a path, find the root installation directory */
  cmd1 = strdup(cmd1);
  bindir = dirname(cmd1);
  if (strcmp(bindir, ".") == 0) {
    rootdir_rel = "..";
  } 
  else {
    bindir_tmp = strdup(bindir);
    rootdir_rel = dirname(bindir_tmp);
  }  

  // printf("%s // %s // %s\n", cmd, bindir, rootdir_rel);
      
  if (realpath(rootdir_rel, rootdir) == NULL) {
    // fprintf(stderr, "%s: %s\n", cmd, strerror(errno));
    goto error;
  }
  
  free(cmd1);
  free(bindir_tmp);
  return rootdir;
  
 error:
  return NULL;
}

/****************************************************************************/
