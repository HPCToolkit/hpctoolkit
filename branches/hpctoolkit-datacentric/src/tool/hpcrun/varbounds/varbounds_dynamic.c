// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/varbounds/varbounds_dynamic.c $
// $Id: varbounds_dynamic.c 4097 2013-02-07 23:03:45Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
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

//=====================================================================
// File: varbounds_dynamic.c  
// 
//     provide information about function bounds for functions in
//     dynamically linked load modules. use an extra "server" process
//     to handle computing the symbols to insulate the client process
//     from the complexity of this task, including use of the system
//     command to fork new processes. having the server process
//     enables use to avoid dealing with forking off new processes
//     with system when there might be multiple threads active with
//     sampling enabled.
//
//  Modification history:
//     2008 April 28 - created John Mellor-Crummey
//
//=====================================================================


//*********************************************************************
// system includes
//*********************************************************************

#include <dlfcn.h>     // for dlopen/dlclose
#include <string.h>    // for strcmp, strerror
#include <stdlib.h>    // for getenv
#include <errno.h>     // for errno
#include <stdbool.h>   
#include <sys/param.h> // for PATH_MAX
#include <sys/types.h>
#include <unistd.h>    // getpid

#include <include/hpctoolkit-config.h>

//*********************************************************************
// external libraries
//*********************************************************************

#include <monitor.h>

//*********************************************************************
// local includes
//*********************************************************************

#include "varbounds_interface.h"
#include "varbounds_file_header.h"
#include "varbounds_client.h"
#include "dylib.h"

#include <hpcrun_dlfns.h>
#include <hpcrun_stats.h>
#include <disabled.h>
#include <loadmap.h>
#include <epoch.h>
#include <sample_event.h>
#include <thread_data.h>

#include <unwind/common/ui_tree.h>
#include <messages/messages.h>

#include <lib/prof-lean/spinlock.h>


int 
varbounds_init()
{
  if (hpcrun_get_disabled()) return 0;

  hpcrun_syserv_var_init();

  return 0;
}


bool
varbounds_enclosing_addr(void* ip, void** start, void** end, load_module_t** lm)
{
  return false;
}


void 
varbounds_fini()
{
  if (hpcrun_get_disabled()) return;

  hpcrun_syserv_var_fini();
}

