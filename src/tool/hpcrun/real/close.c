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
// File: close.c 
//  
// Purpose: 
//   wrapper for libc close to avoid interception.
//------------------------------------------------------------------------------


//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <unistd.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif



//******************************************************************************
// local includes
//******************************************************************************

#include <real/close.h>

#include <monitor-exts/monitor_ext.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef int close_fn_t 
(
 int fd
);



//******************************************************************************
// local data
//******************************************************************************

#ifdef HPCRUN_STATIC_LINK
extern close_fn_t  __real_close;
#endif

static close_fn_t *real_close = NULL;



//******************************************************************************
// local operations
//******************************************************************************

static void 
find_close(void)
{
#ifdef HPCRUN_STATIC_LINK
  real_close = __real_close;
#else
  // don't just look for the next symbol, get it from the source
  void *libc = monitor_real_dlopen("libc.so", RTLD_LAZY);
  real_close = (close_fn_t *) dlsym(libc, "close");
#endif

  assert(real_close);
}



//******************************************************************************
// interface operations
//******************************************************************************

int 
hpcrun_real_close
(
 int fd 
)
{
  static pthread_once_t initialized = PTHREAD_ONCE_INIT;
  pthread_once(&initialized, find_close);
  
  // call the real libc close operation without getting intercepted
  int ret = (* real_close) (fd);

  return ret;
}
