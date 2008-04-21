// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    csproflib.h
//
// Purpose:
//    Interface for explicitly starting/stopping profiling.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//***************************************************************************
//
// There are two interfaces to the profiler:
//
//   1. Implicit: The library will be automatically loaded and
//   initialized. (This depends on LD_PRELOAD and the and the _init
//   and _fini routines.)  The benefit of this interface is that it
//   does not require any binary rewriting.  To use this interface,
//   simply use the appropriate launching script described in the
//   documentation.
//
//   2. Explicit: You can manually call csprof_init() and
//   csprof_fini() or use the the appropriate launching program
//   described in the documentation.  This may be preferable if you
//   cannot or do not wish to use LD_PRELOAD.  It is also useful for
//   debugging.
//
//***************************************************************************
				   
#ifndef CSPROFLIB_H
#define CSPROFLIB_H

#if defined(__cplusplus)
extern "C" {
#endif

  // Explicit interface
  void csprof_init();
  void csprof_fini();

  int csprof_is_handling_sample();
  
#if defined(__cplusplus)
} /* extern "C" */
#endif

//***************************************************************************

#endif
