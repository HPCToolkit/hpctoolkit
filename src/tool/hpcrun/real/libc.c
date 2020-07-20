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
// Copyright ((c)) 2002-2020, Rice University
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

//------------------------------------------------------------------------------
// File: libc.c 
//  
// Purpose: 
//   find the real libc so we can arrange to call functions without 
//   interception.
//------------------------------------------------------------------------------


//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <monitor.h>
#include <monitor-exts/monitor_ext.h>



//******************************************************************************
// macros
//******************************************************************************

#define LIBC_NAME "libc.so.6"



//******************************************************************************
// local data
//******************************************************************************

static void *real_libc = NULL;



//******************************************************************************
// interface operations
//******************************************************************************

static void
find_libc(void)
{
  real_libc = monitor_real_dlopen(LIBC_NAME, RTLD_LAZY);
  assert(real_libc);
}



//******************************************************************************
// interface operations
//******************************************************************************

void * 
hpcrun_real_libc
(
 void
)
{
  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, find_libc);
  
  return real_libc;
}
