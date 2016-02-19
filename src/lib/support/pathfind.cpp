// -*-Mode: C++;-*-

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
  static char* result = NULL;
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
  int	i;
  char*	res;
  
  for (i = 1; i < argc; i++) {
    res = pathfind (getenv ("PATH"), argv[i], "r");
    printf("findpath(%s) == %s\n", argv[i], res ? res : "<<NULL>>");
  }
}
#endif
