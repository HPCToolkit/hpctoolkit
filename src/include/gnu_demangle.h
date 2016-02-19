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

/****************************************************************************
 *
 * File:
 *    gnu_demangle.h
 *
 * Purpose:
 *    [The purpose of this file]
 *
 * Description:
 *    [The set of functions, macros, etc. defined in the file]
 *
 ****************************************************************************/

#ifndef gnu_demangle_H 
#define gnu_demangle_H

/*****************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

  /* We need to avoid name conflicts with GNU's 'cplus_demangle' and,
     e.g., Sun's demangler of the same name.  In order to do so, we
     rename GNU's demangler to 'gnu_cplus_demangle'.  We rename the
     prototype here; the object file's symbol is renamed using
     'objcopy --redefine-sym'.

     Note: GNU_CPLUS_DEMANGLE is the user interface to the GNU
     'cplus_demangle' function.  */

# if defined(__sun)
#   define cplus_demangle     gnu_cplus_demangle /* rename in gnu header */
#   define GNU_CPLUS_DEMANGLE gnu_cplus_demangle /* public handle */
# else
#   define GNU_CPLUS_DEMANGLE cplus_demangle
# endif
  
# include <demangle.h>

# if defined(cplus_demangle)
#   undef cplus_demangle
# endif  

#if defined(__cplusplus)
} /* extern "C" */
#endif

/* Undo possibly mischevious macros in binutils/include/ansidecl.h */
#undef inline

/*****************************************************************************/

#endif 
