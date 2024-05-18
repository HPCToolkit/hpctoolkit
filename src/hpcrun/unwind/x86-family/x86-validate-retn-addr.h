// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef X86_VALIDATE_RETN_ADDR_H
#define X86_VALIDATE_RETN_ADDR_H

#include "../common/validate_return_addr.h"

extern validation_status validate_return_addr(void *addr, void *generic);
extern validation_status deep_validate_return_addr(void *addr, void *generic);

#endif // X86_VALIDATE_RETN_ADDR_H
