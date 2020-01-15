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
// Copyright ((c)) 2002-2020, Rice University
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

#ifndef gpu_channel_util_h
#define gpu_channel_util_h



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/bichannel.h>
#include <lib/prof-lean/stacks.h>



//******************************************************************************
// macros
//******************************************************************************

#define channel_item_alloc(channel, channel_item_type)		\
  (channel_item_type *) channel_item_alloc_helper		\
  ((bichannel_t *) channel, sizeof(channel_item_type))	

#define channel_item_free(channel, item)			\
    channel_item_free_helper					\
    ((bichannel_t *) channel,					\
     (s_element_t *) item)



//******************************************************************************
// interface functions
//******************************************************************************

s_element_t *
channel_item_alloc_helper
(
 bichannel_t *c, 
 size_t size
);


void
channel_item_free_helper
(
 bichannel_t *c, 
 s_element_t *se
);



#endif
