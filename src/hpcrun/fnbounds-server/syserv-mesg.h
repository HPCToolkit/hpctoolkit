// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
  SYSERV_ERR,
  SYSERV_READY
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
