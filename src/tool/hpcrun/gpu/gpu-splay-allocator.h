// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
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

#ifndef gpu_splay_allocator_h
#define gpu_splay_allocator_h


//******************************************************************************
// global includes
//******************************************************************************

#include <stddef.h>	// size_t



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/splay-uint64.h>



//******************************************************************************
// macros
//******************************************************************************

#define typed_splay_alloc(free_list, splay_node_type)		\
  (splay_node_type *) splay_uint64_alloc_helper		\
  ((splay_uint64_node_t **) free_list, sizeof(splay_node_type))	


#define typed_splay_free(free_list, node)			\
  splay_uint64_free_helper					\
  ((splay_uint64_node_t **) free_list,				\
   (splay_uint64_node_t *) node)



//******************************************************************************
// interface functions
//******************************************************************************

splay_uint64_node_t *
splay_uint64_alloc_helper
(
 splay_uint64_node_t **free_list, 
 size_t size
);


void
splay_uint64_free_helper
(
 splay_uint64_node_t **free_list, 
 splay_uint64_node_t *e 
);



#endif
