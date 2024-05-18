// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
