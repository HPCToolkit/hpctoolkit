// SPDX-FileCopyrightText: 2012-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _MCONTEXT_H
#define _MCONTEXT_H

#include <ucontext.h>

static inline void*
ucontext_pc(ucontext_t* context)
{
  return (void*) context->_u._mc.sc_ip;
}

#endif // _MCONTEXT_H
