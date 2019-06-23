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

// This file is a wrapper around the binutils header files designed to
// smooth out some quirks in their files, some real, some historical.
//
// Files that want to use <bfd.h> and <bfdlink.h> should probably
// include this file instead.

//***************************************************************************

#ifndef gnu_bfd_h
#define gnu_bfd_h

// bfd.h (incorrectly) assumes that config.h has already been included
#ifndef PACKAGE
#define PACKAGE  "hpctoolkit"
#endif

/* binutils/bfd/bfd.h only correctly tests for GNU compilers */
#define TRUE_FALSE_ALREADY_DEFINED

#include <bfd.h>
#include <bfdlink.h>

/* Undo possibly mischevious macros in binutils/include/ansidecl.h */
#undef inline

#endif
