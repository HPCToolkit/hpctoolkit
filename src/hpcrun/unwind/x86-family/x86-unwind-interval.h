// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef INTERVALS_H
#define INTERVALS_H

#include <stdbool.h>

#include "../common/binarytree_uwi.h"

/******************************************************************************
 * macros
 ******************************************************************************/

#define lstartaddr ((unsigned long) startaddr)
#define lendaddr ((unsigned long) endaddr)

/*
 * macros to convert the old unwind interval data structure
 *

struct unwind_interval_t {
  struct splay_interval_s common;
  ra_loc ra_status;
  int sp_ra_pos;
  int sp_bp_pos;
  bp_loc bp_status;
  int bp_ra_pos;
  int bp_bp_pos;
  struct unwind_interval_t *prev_canonical;
  int restored_canonical;
  bool has_tail_calls;
};
typedef struct unwind_interval_t unwind_interval;

 * to the new one, which is bitree_uwi_t, a binary tree of uwi_t.
 *
 */

#define UWI_RECIPE(btuwi) ((x86recipe_t*)bitree_uwi_recipe(btuwi))


/*************************************************************************************
 * type declarations
 ************************************************************************************/

typedef bitree_uwi_t unwind_interval;

typedef enum {
  RA_SP_RELATIVE, RA_STD_FRAME, RA_BP_FRAME, RA_REGISTER, POISON
} ra_loc;

typedef enum {
  BP_UNCHANGED, BP_SAVED, BP_HOSED
} bp_loc;

typedef struct x86registers_s {
  int sp_ra_pos; /* return address offset from sp */
  int sp_bp_pos; /* BP offset from sp */

  bp_loc bp_status; /* how to find the bp register */

  int bp_ra_pos; /* return address offset from bp */
  int bp_bp_pos; /* (caller's) BP offset from bp */
} x86registers_t;

typedef struct x86recipe_s {
  ra_loc ra_status; /* how to find the return address */
  x86registers_t reg;

  bitree_uwi_t* prev_canonical;

  bool has_tail_calls;
} x86recipe_t;


/*************************************************************************************
 * interface operations
 ************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/unwind-interval.h"

  void set_ui_canonical(unwind_interval *u, unwind_interval *value);

  unwind_interval *
  new_ui(char *startaddr, ra_loc ra_status, const x86registers_t *reg);

  unwind_interval *fluke_ui(char *pc,unsigned int sp_ra_pos);

  void link_ui(unwind_interval *current, unwind_interval *next);
  void dump_ui(unwind_interval *u, int dump_to_stderr);
  void dump_ui_stderr(unwind_interval *u);
  void dump_ui_log(unwind_interval *u);
  void dump_ui_dbg(unwind_interval *u);
  void dump_ui_troll(unwind_interval *u);

  void suspicious_interval(void *pc);

  /*
   * Concrete implementation of the abstract val_tostr function of the
   * generic_val class.
   * pre-condition: recipe is of type x86recipe_t*
   */
  void
  x86recipe_tostr(void* recipe, char str[]);

  void
  x86recipe_print(void* recipe);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif // INTERVALS_H
