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

#ifndef CCT_DUMP_H
#define CCT_DUMP_H
#ifndef _FLG
#define _FLG CCT
#endif

#include <stdint.h>

#include <cct/cct.h>
#include <messages/messages.h>

#define TLIM 1

#ifndef TLIM
#define TL(E)  E
#define TLS(S) S
#else
#include <hpcrun/thread_data.h>
#define TL(E) ((TD_GET(id) == TLIM) && (E))
#define TLS(S) if (TD_GET(id) == TLIM) S
#endif


#define TF(_FLG, ...) TMSG(_FLG, __VA_ARGS__)

static void
l_dump(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  char p[level+1];

  if (! node) return;

  memset(&p[0], ' ', level);
  p[level] = '\0';

  int32_t id = hpcrun_cct_persistent_id(node);
  cct_addr_t* adr = hpcrun_cct_addr(node);

  TF(_FLG, "%s[%d: (%d, %p)]", &p[0], id,
     adr->ip_norm.lm_id, adr->ip_norm.lm_ip);
}

static void
dump_cct(cct_node_t* cct)
{
  hpcrun_cct_walk_node_1st(cct, l_dump, NULL);
}

#endif // CCT_DUMP_H
