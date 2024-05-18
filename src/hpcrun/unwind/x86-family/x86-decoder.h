// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_decoder_h
#define x86_decoder_h

/******************************************************************************
 * include files
 *****************************************************************************/

#if __has_include(<xed/xed-interface.h>)
#include <xed/xed-interface.h>
#else
#include <xed-interface.h>
#endif

/******************************************************************************
 * type declarations
 *****************************************************************************/

typedef struct {
  xed_state_t xed_settings;
} xed_control_t;


/******************************************************************************
 * macros
 *****************************************************************************/

#define iclass(xptr) xed_decoded_inst_get_iclass(xptr)

#define iclass_eq(xptr, class) (iclass(xptr) == (class))



/******************************************************************************
 * global variables
 *****************************************************************************/

extern xed_control_t x86_decoder_settings;



/******************************************************************************
 * interface operations
 *****************************************************************************/
void x86_family_decoder_init();

#endif
