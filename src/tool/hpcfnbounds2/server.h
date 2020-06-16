// -*-Mode: C++;-*-

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
#ifndef _FNBOUNDS_SERVER_H_
#define _FNBOUNDS_SERVER_H_

#include <stdint.h>
#include "code-ranges.h"
#include "function-entries.h"
#include "syserv-mesg.h"

uint64_t	init_server(DiscoverFnTy, int, int);
void	do_query(DiscoverFnTy , struct syserv_mesg *);
void  send_funcs();

void	signal_handler_init();
int	read_all(int, void*, size_t);
int	write_all(int, const void*, size_t);
int	read_mesg(struct syserv_mesg *mesg);
int	write_mesg(int32_t type, int64_t len);
void	signal_handler(int);

#if 0
void syserv_add_addr(void *, long);
void syserv_add_header(int is_relocatable, uintptr_t ref_offset);
#endif

#endif  // _FNBOUNDS_SERVER_H_
