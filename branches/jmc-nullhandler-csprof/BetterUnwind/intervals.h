#ifndef INTERVALS_H
#define INTERVALS_H
typedef  enum {RA_SP_RELATIVE, RA_BP_RELATIVE, RA_REGISTER} ra_loc; 
struct unwind_interval_t {
  unsigned long startaddr;
  unsigned long endaddr;
  ra_loc ra_status;
  unsigned int ra_pos; /* SP or BP offset, or register number */
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
  unwind_interval *fluke_interval(char *pc,unsigned int ra_pos);
#ifdef __cplusplus
};
#endif

#endif
