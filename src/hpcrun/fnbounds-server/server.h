// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef _FNBOUNDS_SERVER_H_
#define _FNBOUNDS_SERVER_H_

#include <stdint.h>
#include "code-ranges.h"
#include "function-entries.h"
#include "syserv-mesg.h"

uint64_t        init_server(DiscoverFnTy, int, int);
void    do_query(DiscoverFnTy , struct syserv_mesg *);
void  send_funcs();

void    signal_handler_init();
int     read_all(int, void*, size_t);
int     write_all(int, const void*, size_t);
int     read_mesg(struct syserv_mesg *mesg);
int     write_mesg(int32_t type, int64_t len);
void    signal_handler(int);

#endif  // _FNBOUNDS_SERVER_H_
