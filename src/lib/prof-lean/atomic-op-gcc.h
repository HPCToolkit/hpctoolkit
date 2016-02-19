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

#ifndef atomic_op_gcc_h
#define atomic_op_gcc_h

//***************************************************************************
//
// File: atomic-op-gcc.h 
//
// Description:
//   implement the small set of atomic operations needed by hpctoolkit
//   using gcc atomics
//
// Author:
//   23 July 2009 - John Mellor-Crummey - created
//
//***************************************************************************

//***************************************************************************
// macros
//***************************************************************************

#define fetch_and_add(addr, val) \
	__sync_fetch_and_add(addr, val) 

#define fetch_and_add_i32(addr, val) \
	fetch_and_add(addr, val) 

#define fetch_and_add_i64(addr, val) \
	fetch_and_add(addr, val) 


#define atomic_add(addr, val) \
	(void) fetch_and_add(addr, val) 

#define atomic_add_i32(addr, val) \
	atomic_add(addr, val) 

#define atomic_add_i64(addr, val) \
	atomic_add(addr, val) 



// -------------------------------------------------------------
// requirement: _sync_lock_test_and_set(addr, newval) must 
// implement an exchange on your architecture. if not, this 
// won't work.
// -------------------------------------------------------------
#define fetch_and_store(addr, newval)  \
	__sync_lock_test_and_set(addr, newval) 

#define fetch_and_store_i32(addr, newval) \
	fetch_and_store(addr, newval) 

#define fetch_and_store_i64(addr, newval) \
	fetch_and_store(addr, newval) 

#define fetch_and_store_ptr(addr, newval) \
	fetch_and_store(addr, newval)


#define compare_and_swap(addr, oldval, newval) \
	__sync_val_compare_and_swap(addr, oldval, newval)

#define compare_and_swap_i32(addr, oldval, newval) \
	compare_and_swap(addr, oldval, newval) 

#define compare_and_swap_i64(addr, oldval, newval) \
	compare_and_swap(addr, oldval, newval) 

#define compare_and_swap_ptr(addr, oldval, newval) \
	compare_and_swap(addr, oldval, newval) 

#define compare_and_swap_bool(addr, oldval, newval) \
	__sync_bool_compare_and_swap(addr, oldval, newval)


#endif
