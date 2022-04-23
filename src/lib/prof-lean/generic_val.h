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

/*
 * generic_val.h
 *
 * A variable of type void* epresents an abstract notion of a value in a tree
 * or list node.
 *
 *      Author: dxnguyen
 */

#ifndef __GENERIC_VAL__
#define __GENERIC_VAL__

#if 0
// abstract function to compute and return a string representation of
// a generic value.
typedef char* (*val_str)(void* val);
#endif

// abstract function to compute a string representation of a generic value
// and return the result in the str parameter.
// caller to provide the appropriate length for the result string.
typedef void (*val_tostr)(void* val, char str[]);

/*
 * Abstract comparator function to compare two generic values: lhs vs rhs
 * Returns:
 * 0 if lhs matches rhs
 * < 0 if lhs < rhs
 * > 0 if lhs > rhs
 */
typedef int (*val_cmp)(void* lhs, void* rhs);

#endif /* __GENERIC_VAL__ */
