#ifndef INTERVALS_H
#define INTERVALS_H

#include <stdbool.h>

#include "splay-interval.h"

/*************************************************************************************
 * type declarations 
 ************************************************************************************/
typedef enum {
  RA_SP_RELATIVE, RA_STD_FRAME, RA_BP_FRAME, RA_REGISTER, POISON
} ra_loc;

typedef enum {
  BP_UNCHANGED, BP_SAVED, BP_HOSED
} bp_loc;

struct unwind_interval_t {
  struct splay_interval_s common; // common splay tree fields

  ra_loc ra_status; /* how to find the return address */

  int sp_ra_pos; /* return address offset from sp */
  int sp_bp_pos; /* BP offset from sp */

  bp_loc bp_status; /* how to find the bp register */

  int bp_ra_pos; /* return address offset from bp */
  int bp_bp_pos; /* (caller's) BP offset from bp */

  struct unwind_interval_t *prev_canonical;
  int restored_canonical;

  bool has_tail_calls;
};

#define lstartaddr ((unsigned long) startaddr)
#define lendaddr ((unsigned long) endaddr)

typedef struct unwind_interval_t unwind_interval;

/*************************************************************************************
 * global variables 
 ************************************************************************************/
extern const unwind_interval poison_ui;


/*************************************************************************************
 * interface operations
 ************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif


  void set_ui_canonical(unwind_interval *u, unwind_interval *value);

  void set_ui_restored_canonical(unwind_interval *u, unwind_interval *value);


  interval_status build_intervals(char  *ins, unsigned int len);

  unwind_interval *
  new_ui(char *startaddr, 
	 ra_loc ra_status, unsigned int sp_ra_pos, int bp_ra_pos, 
	 bp_loc bp_status,          int sp_bp_pos, int bp_bp_pos,
	 unwind_interval *prev);

  unwind_interval *fluke_ui(char *pc,unsigned int sp_ra_pos);

  void link_ui(unwind_interval *current, unwind_interval *next);
  void dump_ui(unwind_interval *u, int dump_to_stderr);
  void dump_ui_stderr(unwind_interval *u);
  void dump_ui_log(unwind_interval *u);
  void dump_ui_dbg(unwind_interval *u);
  void dump_ui_troll(unwind_interval *u);
  long ui_count();

  long suspicious_count();
  void suspicious_interval(void *pc);

#ifdef __cplusplus
};
#endif



#endif // INTERVALS_H
