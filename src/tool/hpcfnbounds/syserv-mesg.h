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

// This file defines the API for messages over the pipe between the
// hpcrun client (hpcrun/fnbounds/fnbounds_client.c) and the new
// fnbounds server (server.cpp).
//
// Note: none of these structs needs to be platform-independent
// because they're only used between processes within a single node
// (same for the old server).

//***************************************************************************

#ifndef _SYSERV_MESG_H_
#define _SYSERV_MESG_H_

#include <stdint.h>

#define SYSERV_MAGIC    0x00f8f8f8
#define FNBOUNDS_MAGIC  0x00f9f9f9

enum {
  SYSERV_ACK = 1,
  SYSERV_QUERY,
  SYSERV_EXIT,
  SYSERV_OK,
  SYSERV_ERR
};

struct syserv_mesg {
  int32_t  magic;
  int32_t  type;
  int64_t  len;
};

struct syserv_fnbounds_info {
  // internal fields for the client
  int32_t   magic;
  int32_t   status;
  long      memsize;

  // fields for the fnbounds file header
  uint64_t  num_entries;
  uint64_t  reference_offset;
  int       is_relocatable;
};

#endif  // _SYSERV_MESG_H_
