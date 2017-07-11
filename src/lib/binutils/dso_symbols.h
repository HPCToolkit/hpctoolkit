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
// Copyright ((c)) 2002-2017, Rice University
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

//***************************************************************************
//
// File:
//   dso_symbols.h
//
// Purpose:
//   interface for extracting dynamic symbols from a shared library
//
// Description:
//   extract dynamic symbols from a shared library. this needs to be done
//   at runtime to understand VDSOs.
//
//
//***************************************************************************

#ifndef __DSO_SYMBOLS_H__
#define __DSO_SYMBOLS_H__

#ifdef __cplusplus
extern "C" {
#endif

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef enum dso_symbol_bind_e {
  dso_symbol_bind_local,
  dso_symbol_bind_global,
  dso_symbol_bind_weak,
  dso_symbol_bind_other
} dso_symbol_bind_t;


typedef
void
(dso_symbols_symbol_callback_t)
(
 const char *func_sym_name,
 int64_t func_sym_addr,
 dso_symbol_bind_t func_sym_bind,
 void *callback_arg // closure state
);



//******************************************************************************
// interface functions
//******************************************************************************

int
dso_symbols_vdso
(
 dso_symbols_symbol_callback_t note_symbol, // callback
 void *callback_arg                         // closure state
);


int
dso_symbols
(
 const char *loadmodule_pathname,
 dso_symbols_symbol_callback_t note_symbol, // callback
 void *callback_arg                         // closure state
);


#ifdef __cplusplus
};
#endif

#endif
