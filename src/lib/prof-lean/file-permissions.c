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


//******************************************************************************
// system includes
//******************************************************************************

#include <limits.h>
#include <unistd.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "file-permissions.h"



//******************************************************************************
// macros
//******************************************************************************

// a file or directory has at least the required access permission
#define PERMISSIONS_MATCH(mode, perm) ((mode & perm) == perm) 



//******************************************************************************
// interface operations
//******************************************************************************

//----------------------------------------------------------------------
// function: file_permissions_perm_select
//
// description:
//   helper function for assembling requested file mode at runtime
//----------------------------------------------------------------------
mode_t 
file_permissions_perm_select
(
 int perm,
 mode_t read,
 mode_t write,
 mode_t execute
)
{
  mode_t retval = 0;

  if (perm & PERM_R) retval |= read;
  if (perm & PERM_W) retval |= write;
  if (perm & PERM_X) retval |= execute;

  return retval;
}


//----------------------------------------------------------------------
// function: file_permissions_file_accessible
//
// description:
//   check whether a file or directory is readable and, if necessary, 
//   writable by checking its permissions in the file or directory's 
//   stat 'buf'
//
// return values:
//    0 on failure, 1 on success
//----------------------------------------------------------------------
int
file_permissions_file_accessible
(
 struct stat *buf,
 int perm
)
{
  int is_accessible = 0;

  mode_t mode = buf->st_mode;

  if (PERMISSIONS_MATCH(mode, OTHER_PERM(perm))) { // accessible by anyone
    is_accessible = 1;
  } else {
    if (file_permissions_group_ismember(buf->st_gid)) { // one of my groups
      if (PERMISSIONS_MATCH(mode, GROUP_PERM(perm))) { // accessible by group
	is_accessible = 1;
      } else {
	if (file_permissions_owner_matches(buf->st_uid)) { 
	  if (PERMISSIONS_MATCH(mode, USER_PERM(perm))) { // accessible by owner
	    is_accessible = 1;
	  }
	}
      }
    }
  }

  return is_accessible;
}


//----------------------------------------------------------------------
// function: file_permissions_owner_matches
//
// description:
//   check if the fileuid is matches me
//
// return values:
//    0 on failure, 1 on success
//----------------------------------------------------------------------
int
file_permissions_owner_matches
(
 uid_t fileuid
)
{
  uid_t my_id = getuid();

  return fileuid == my_id;
}


//----------------------------------------------------------------------
// function: file_permissions_group_ismember
//
// description:
//   check if the filegroup is one of mine
//
// return values:
//    0 on failure, 1 on success
//----------------------------------------------------------------------
int
file_permissions_group_ismember
(
 gid_t filegroup
)
{
  gid_t groups[NGROUPS_MAX];

  // get my groups, returning the count
  int ngroups = getgroups(NGROUPS_MAX, groups);

  int i;
  for (i=0; i < ngroups; i++) {
    if (filegroup == groups[i]) return 1;
  }

  return 0;
}


//----------------------------------------------------------------------
// function: file_permissions_file_readable
//
// description:
//   check whether a file is a regular file and is readable.
//   
// return values:
//    0 on failure, 1 on success
//----------------------------------------------------------------------
int
file_permissions_contain
(
 const char *filepath,
 int perm
)
{
  int is_available = 0;

  struct stat buf;

  if (stat(filepath, &buf) == 0) {
    if (S_ISREG(buf.st_mode)) {
      // file exists 
      if (file_permissions_file_accessible(&buf, perm)) {
	// file is readable
	is_available = 1;
      }
    }
  } 

  return is_available;
}
