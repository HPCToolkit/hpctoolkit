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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef PPCAT_PPSTR_H
#define PPCAT_PPSTR_H

// Add several Levels of indirection for cpp token pasting (##),
// and stringize (#) operators.
// These macros are to facilitate other macro definitions that use these features.
// (The indirection is necessary due to quirkyness of cpp expansion algorithm).
//
// If another level of indirection is needed, then add another line
// to the auxiliary functions, and change the main definition to
// use the new level.
//
// Example:
//   Add line
//     #define __CAT3_(a, b) __CAT2_(a, b)
//
//  Then change definition of PPCAT to
//     #define PPCAT(a, b) __CAT3_(a, b)

#define __CAT0_(a, b) a##b
#define __CAT1_(a, b) __CAT0_(a, b)
#define __CAT2_(a, b) __CAT1_(a, b)

#define PPCAT(a, b) __CAT2_(a, b)

#define __STR0_(a) #a
#define __STR1_(a) __STR0_(a)
#define __STR2_(a) __STR1_(a)

#define PPSTR(a) __STR2_(a)

#endif  // PPCAT_PPSTR_H
