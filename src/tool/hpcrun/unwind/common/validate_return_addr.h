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

#ifndef VALIDATE_RETURN_ADDR_H
#define VALIDATE_RETURN_ADDR_H

#undef _MM
#define _MM(a,v) UNW_ADDR_ ## a = v

typedef enum {
#include "validate_return_addr.src"
} validation_status;

#undef _MM
#define _MM(a,v) [UNW_ADDR_ ## a] = "UNW_ADDR_" #a

static char *_vstat2str_tbl[] = {
#include "validate_return_addr.src"
};

static inline
char *vstat2s(validation_status v) { return _vstat2str_tbl[v]; }

typedef validation_status (*validate_addr_fn_t)(void *addr, void *generic_arg);
extern void hpcrun_validation_summary(void);

#endif // VALIDATE_RETURN_ADDR_H
