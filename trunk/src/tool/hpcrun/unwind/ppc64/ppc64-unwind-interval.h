#ifndef ppc64_unwind_interval_h
#define ppc64_unwind_interval_h


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "splay-interval.h"



/******************************************************************************
 * type declarations 
 *****************************************************************************/

typedef enum {
  RA_SP_RELATIVE, RA_STD_FRAME, RA_BP_FRAME, RA_REGISTER, POISON
} ra_loc;


typedef enum {
  BP_UNCHANGED, BP_SAVED, BP_HOSED
} bp_loc;


struct unwind_interval_t {
  struct splay_interval_s common; // common splay tree fields

  ra_loc ra_status; /* how to find the return address */
  bp_loc bp_status; /* how to find the bp register */
  int bp_ra_pos;    /* return address offset from bp */
};


typedef struct unwind_interval_t unwind_interval;



/******************************************************************************
 * global variables 
 *****************************************************************************/
extern const unwind_interval poison_ui;



/******************************************************************************
 * interface operations
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

  interval_status build_intervals(char  *ins, unsigned int len);

  unwind_interval *
  new_ui(char *startaddr, ra_loc ra_status, int bp_ra_pos, 
	 bp_loc bp_status, unwind_interval *prev);


  void link_ui(unwind_interval *current, unwind_interval *next);
  void dump_ui(unwind_interval *u, int dump_to_stdout);

  long ui_count();

  long suspicious_count();
  void suspicious_interval(void *pc);

#ifdef __cplusplus
};
#endif



#endif // ppc64_unwind_interval_h
