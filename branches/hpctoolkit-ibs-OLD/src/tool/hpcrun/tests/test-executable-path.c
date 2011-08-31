// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <stdio.h>
#include <limits.h>
#include "executable-path.h"

int harness(const char *filename, const char *path_list);

int main(int argc, char **argv)
{
  char path[PATH_MAX];
  const char *ld_library_path =  ".:/lib64:/usr/lib";
  char *self = "test-executable-path";
  int result = 0;

  /* relative path name */
  result |= harness(self, ld_library_path);

  /* absolute path name */
  if (realpath(self, path)) harness(path, ld_library_path);
  else {
    result = 1;
    printf("realpath failed to compute absolute path for this executable!\n");
  }
  result |= harness("libm.so", ld_library_path);
  result |= harness("libdl.so", ld_library_path);
  return result ? -1: 0;
}

int harness(const char *filename, const char *path_list)
{
  char executable[PATH_MAX];
  char *result = executable_path(filename, path_list, executable);
  printf("search for executable '%s' using path '%s', result = '%s'\n", 
	 filename, path_list, result);
  return (result == NULL);
}
