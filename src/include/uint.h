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

//***************************************************************************
//
// File: 
//   $HeadURL$
//
// Purpose:
//   uint types
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef include_uint_h
#define include_uint_h

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

//****************************************************************************

// A sanity check that should probably just be in configure (FIXME)
#if !defined(SIZEOF_VOIDP)
# error "configure.ac should have defined SIZEOF_VOIDP using AC_CHECK_SIZEOF."
#endif


#if 0

#if defined(__cplusplus)
# include <cstddef> // for 'NULL'
#else
# include <stddef.h>
#endif

#endif

//****************************************************************************
// Convenient unsigned types
//****************************************************************************

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif


#if defined(__cplusplus)

# if !defined(HAVE_USHORT)
  typedef    unsigned short int    ushort;
# endif

# if !defined(HAVE_UINT)
  typedef    unsigned int          uint;
# endif

# if !defined(HAVE_ULONG)
  typedef    unsigned long int     ulong;
# endif

#else

# if !defined(HAVE_USHORT_LANG_C)
  typedef    unsigned short int    ushort;
# endif

# if !defined(HAVE_UINT_LANG_C)
  typedef    unsigned int          uint;
# endif

# if !defined(HAVE_ULONG_LANG_C)
  typedef    unsigned long int     ulong;
# endif

#endif


//***************************************************************************

#endif /* include_uint_h */
