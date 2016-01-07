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

#ifndef __MCONTEXT_H
#define __MCONTEXT_H

#include <ucontext.h>
#include <sys/ucontext.h>

#define GET_MCONTEXT(context) (&((ucontext_t *)context)->uc_mcontext)

//-------------------------------------------------------------------------
// define macros for extracting pc, bp, and sp from machine contexts. these
// macros bridge differences between machine context representations for
// various architectures
//-------------------------------------------------------------------------

#ifdef REG_RIP
#define REG_INST_PTR REG_RIP
#define REG_BASE_PTR REG_RBP
#define REG_STACK_PTR REG_RSP
#else

#ifdef REG_EIP
#define REG_INST_PTR REG_EIP
#define REG_BASE_PTR REG_EBP
#define REG_STACK_PTR REG_ESP
#else 
#ifdef REG_IP
#define REG_INST_PTR REG_IP
#define REG_BASE_PTR REG_BP
#define REG_STACK_PTR REG_SP
#endif
#endif
#endif


#define MCONTEXT_REG(mctxt, reg) (mctxt->gregs[reg])

#define LV_MCONTEXT_PC(mctxt)         MCONTEXT_REG(mctxt, REG_INST_PTR)
#define LV_MCONTEXT_BP(mctxt)         MCONTEXT_REG(mctxt, REG_BASE_PTR)
#define LV_MCONTEXT_SP(mctxt)         MCONTEXT_REG(mctxt, REG_STACK_PTR)

#define MCONTEXT_PC(mctxt) ((void *)  MCONTEXT_REG(mctxt, REG_INST_PTR))
#define MCONTEXT_BP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_BASE_PTR))
#define MCONTEXT_SP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_STACK_PTR))

#endif // __MCONTEXT_H
