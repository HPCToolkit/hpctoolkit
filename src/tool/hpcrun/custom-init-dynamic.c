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

//
// run a special user initialization function, if the CUSTOM_INIT flag is set
//
// Dynamic case uses dl functions to open a canonically named shared object file.
// extract the "hpcrun_custom_init" function, and call it.
//
// The canonical name for the shared object is "hpcrun-custom.so". At the moment, this
// file must be found in the same directory as the one in which hpcrun is invoked.
// 
//

// *********************************************
// system includes
// *********************************************

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <dlfcn.h>

// *********************************************
// local includes
// *********************************************

#include "custom-init.h"
#include <monitor.h>
#include <messages/messages.h>

// *********************************************
// interface functions
// *********************************************

void
hpcrun_do_custom_init(void)
{
  if (ENABLED(CUSTOM_INIT)) {
    void* custom_fns = monitor_real_dlopen("./hpcrun-custom.so", RTLD_LAZY);
    if (custom_fns) {
      void (*hpcrun_custom_init)(void) = (void (*)(void)) dlsym(custom_fns, "hpcrun_custom_init");
      if (hpcrun_custom_init) {
	TMSG(CUSTOM_INIT, "Before call to custom_init");
	hpcrun_custom_init();
	TMSG(CUSTOM_INIT, "Return from custom_init");
      }
      else {
	TMSG(CUSTOM_INIT, "could not dynamically load hpcrun_custom_init procedure");
      }
      monitor_real_dlclose(custom_fns);
    }
    else {
      TMSG(CUSTOM_INIT, "could not open hpcrun-custom.so");
    }
  }
}
