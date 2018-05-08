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
// Copyright ((c)) 2002-2018, Rice University
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
// HPCToolkit's PPC64 Unwinder
//
//***************************************************************************

#ifndef ppc64_unwind_interval_h
#define ppc64_unwind_interval_h

//************************* System Include Files ****************************

#include <stddef.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include <unwind/common/binarytree_uwi.h>
#include <unwind/common/unwind-interval.h>

/******************************************************************************
 * macro
 ******************************************************************************/
/*
 * to convert from the old unwind interval data structure
 *

typedef struct {
  struct splay_interval_s common; // common splay tree fields
  // frame type
  sp_ty_t sp_ty : 16;
  ra_ty_t ra_ty : 16;
  // SPTy_Reg  : Parent SP's register
  // RATy_SPRel: Parent SP's offset from appropriate pointer
  int sp_arg;
  // RATy_Reg  : Parent RA's register
  // RATy_SPRel: Parent RA's offset from appropriate pointer
  int ra_arg;
} unw_interval_t;

 * to the new one, which is bitree_uwi_t, a binary tree of uwi_t,
 * and extract the unwind recipe for ppc64.
 *
 */

#define UWI_RECIPE(btuwi) ((ppc64recipe_t*)bitree_uwi_recipe(btuwi))

// same as in x8-unwind-interval.h
typedef bitree_uwi_t unwind_interval;

typedef enum {
  SPTy_NULL = 0,
  SPTy_Reg,     // Parent's SP is in a register (R1) (unallocated frame)
  SPTy_SPRel,   // Parent's SP is relative to current SP (saved in frame)
} sp_ty_t;


typedef enum {
  RATy_NULL = 0,
  RATy_Reg,     // RA is in a register (either LR or R0)
  RATy_SPRel,   // RA is relative to SP
} ra_ty_t;

typedef struct ppc64recipe_s{
  // frame type
  sp_ty_t sp_ty : 16;
  ra_ty_t ra_ty : 16;

  // SPTy_Reg  : Parent SP's register
  // RATy_SPRel: Parent SP's offset from appropriate pointer
  int sp_arg;

  // RATy_Reg  : Parent RA's register
  // RATy_SPRel: Parent RA's offset from appropriate pointer
  int ra_arg;
} ppc64recipe_t;

unwind_interval *
new_ui(
	char *startaddr,
	sp_ty_t sp_ty,
	ra_ty_t ra_ty,
	int sp_arg,
	int ra_arg);

#if 0
ppc64recipe_t *
ppc64recipe_new(sp_ty_t sp_ty, ra_ty_t ra_ty, int sp_arg, int ra_arg);
#endif

/*
 * Concrete implementation of the abstract val_tostr function of the
 * generic_val class.
 * pre-condition: recipe is of type ppc64recipe_t*
 */
void
ppc64recipe_tostr(void* recipe, char str[]);

void
ppc64recipe_print(void* recipe);


static inline bool 
ui_cmp(unwind_interval* ux, unwind_interval* uy)
{
  ppc64recipe_t *x = UWI_RECIPE(ux);
  ppc64recipe_t *y = UWI_RECIPE(uy);
  return ((x->sp_ty  == y->sp_ty) &&
	  (x->ra_ty  == y->ra_ty) && 
	  (x->sp_arg == y->sp_arg) &&
	  (x->ra_arg == y->ra_arg));
}

void 
ui_dump(unwind_interval *u);

// FIXME: these should be part of the common interface
void suspicious_interval(void *pc);
void link_ui(unwind_interval *current, unwind_interval *next);


//***************************************************************************

#endif // ppc64_unwind_interval_h
