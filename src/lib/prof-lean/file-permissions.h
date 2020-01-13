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

#ifndef file_permissions_h
#define file_permissions_h



//******************************************************************************
// system includes
//******************************************************************************

#include <sys/types.h>
#include <sys/stat.h>



//******************************************************************************
// macros
//******************************************************************************

#define PERM_R 1 // select read permission
#define PERM_W 2 // select write permission
#define PERM_X 4 // select execute permission


// shorthand for convenient permission sets
#define RWX (PERM_R | PERM_W | PERM_X)
#define RW_ (PERM_R | PERM_W)
#define R_X (PERM_R | PERM_X)
#define R__ (PERM_R)


// use permission sets to synthesize permissions for user, group, owner
#define OTHER_PERM(perm)					\
  file_permissions_perm_select(perm, S_IROTH, S_IWOTH, S_IXOTH) 
#define GROUP_PERM(perm)					\
  file_permissions_perm_select(perm, S_IRGRP, S_IWGRP, S_IXGRP) 
#define USER_PERM(perm)						\
  file_permissions_perm_select(perm, S_IRUSR, S_IWUSR, S_IXUSR) 


// some useful permissions
#define RWXR_XR_X  (USER_PERM(RWX) | GROUP_PERM(R_X) | OTHER_PERM(R_X)) 
#define RW_R__R__  (USER_PERM(RW_) | GROUP_PERM(R__) | OTHER_PERM(R__)) 



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
);


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
);


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
);


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
);

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
);



#endif
