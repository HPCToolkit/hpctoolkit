// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//************************** System Include Files ***************************

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

#include <unistd.h>

//*************************** User Include Files ****************************

#include "pathfind.h"
#include "CStrUtil.h" // for 'ssave' and 'sfree'

#include "PathFindMgr.hpp"

//*************************** Forward Declarations ***************************

//***************************************************************************


char*
pathfind(const char* pathList,
         const char* name,
         const char* mode)
{
  static __thread char* result = NULL;
  int   accessFlags = 0;
  const char* path;
  const char* sep;

  if (result) { sfree(result); result = NULL; }

  /* Check that name is plausible. */
  if (name == NULL || *name == '\0')
    return NULL;

  /* Convert mode chars to accessFlags, for use by access function. */
  if (mode) {
    if (strchr( mode, 'r' )) accessFlags |= R_OK;
    if (strchr( mode, 'w' )) accessFlags |= W_OK;
    if (strchr( mode, 'x' )) accessFlags |= X_OK;
  }
  if (!accessFlags) { accessFlags = F_OK; }

  /* If pathList is empty or name is absolute, don't search path for it. */
  if (pathList == NULL || *pathList == '\0' || *name == '/') {
    if (access( name, accessFlags ) >= 0) {
      result = ssave(name);
    }
    return result;
  }

  path = pathList;
  while (path) {

    int len, pathLen, retval;
    char* tmp;

    sep = strchr (path, ':');
    pathLen = sep ? sep - path : strlen (path);
    len = strlen (name) + 1;
    if (pathLen) len += pathLen + 1;
    tmp = new char[len];
    if (pathLen) {
      strncpy(tmp, path, pathLen);
      tmp[pathLen] = '/';    // Can't use strcat, since tmp may not
      tmp[pathLen+1] = '\0'; // be NULL-terminated following strncpy.
      strcat (tmp, name);
    }
    else {
      strcpy (tmp, name);
    }

    retval = access( tmp, accessFlags );
#ifdef SELFTEST
    printf ("Access %s == %d\n", tmp, retval);
#endif
    if (retval >= 0) {
      result = tmp;
      return (result);
    }
    else {
      delete[] tmp;
    }

    path = sep ? sep + 1 : NULL;
  }

  return NULL;
}


#ifdef SELFTEST
int
main (int argc, const char *argv[])
{
  int   i;
  char* res;

  for (i = 1; i < argc; i++) {
    res = pathfind (getenv ("PATH"), argv[i], "r");
    printf("findpath(%s) == %s\n", argv[i], res ? res : "<<NULL>>");
  }
}
#endif
