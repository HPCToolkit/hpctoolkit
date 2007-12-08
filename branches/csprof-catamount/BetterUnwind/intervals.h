#ifndef INTERVALS_H
#define INTERVALS_H
typedef  enum {RA_SP_RELATIVE, RA_STD_FRAME, RA_BP_FRAME, RA_REGISTER, POISON} ra_loc; 
typedef  enum {BP_UNCHANGED, BP_SAVED, BP_HOSED} bp_loc;
struct unwind_interval_t {
  unsigned long startaddr;
  unsigned long endaddr;
  ra_loc ra_status; /* how to find the return address */
  int sp_ra_pos; /* return address offset from sp */
  int sp_bp_pos; /* BP offset from sp */
  bp_loc bp_status; /* how to find the bp register */
  int bp_ra_pos; /* return address offset from bp */
  int bp_bp_pos; /* (caller's) BP offset from bp */
  struct unwind_interval_t *next;
  struct unwind_interval_t *prev;
};

typedef struct unwind_interval_t unwind_interval;


typedef struct interval_status_t {
  char *first_undecoded_ins;
  unwind_interval *first;
  int errcode;
} interval_status;

#ifdef __cplusplus
extern "C" {
#endif
  interval_status l_build_intervals(char  *ins, unsigned int len);
  void idump(unwind_interval *u);
  unwind_interval *fluke_interval(char *pc,unsigned int sp_ra_pos);
#ifdef __cplusplus
};
#endif

#endif
