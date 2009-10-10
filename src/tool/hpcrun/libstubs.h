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
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef LIBSTUBS_H
#define LIBSTUBS_H

/* We need to get handles on "original" function pointers for several
   functions.  This file commonizes the necessary frobbing. */

/* grab the function pointer and die if we can't find it */
#ifndef HPCRUN_STATIC_LINK
#define CSPROF_GRAB_FUNCPTR(our_name, platform_name) \
do { \
    hpcrun_ ## our_name = dlsym(RTLD_NEXT, #platform_name); \
    if(hpcrun_ ## our_name == NULL) { \
        printf("Error in locating " #platform_name "\n"); \
        exit(0); \
    } \
} while(0)
#else
#define CSPROF_GRAB_FUNCPTR(our_name, platform_name) \
do { \
    hpcrun_ ## our_name = &platform_name; \
    /* printf("hpcrun_" #our_name " = %p\n",hpcrun_ ## our_name); */ \
} while(0)
#endif
/* we assume that we have a function `init_library_stubs' and a
   `library_stubs_initialized' variable here.  This macro should
   be placed at the entry of any function we override. */
#define MAYBE_INIT_STUBS() do { if(!library_stubs_initialized) { init_library_stubs(); } } while(0)

#endif
