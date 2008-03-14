#ifndef intervals_h
#define intervals_h

/*************************************************************************************
 * type declarations 
 ************************************************************************************/
typedef  enum {RA_SP_RELATIVE, RA_STD_FRAME, RA_BP_FRAME, RA_REGISTER, POISON} ra_loc; 
typedef  enum {BP_UNCHANGED, BP_SAVED, BP_HOSED} bp_loc;
struct unwind_interval_t {
  void * startaddr;
  void * endaddr;
  ra_loc ra_status; /* how to find the return address */
  int sp_ra_pos; /* return address offset from sp */
  int sp_bp_pos; /* BP offset from sp */
  bp_loc bp_status; /* how to find the bp register */
  int bp_ra_pos; /* return address offset from bp */
  int bp_bp_pos; /* (caller's) BP offset from bp */
  struct unwind_interval_t *next;
  struct unwind_interval_t *prev;
};

#define lstartaddr ((unsigned long) startaddr)
#define lendaddr ((unsigned long) endaddr)

typedef struct unwind_interval_t unwind_interval;

typedef struct interval_status_t {
  char *first_undecoded_ins;
  unwind_interval *first;
  int errcode;
} interval_status;



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

  interval_status build_intervals(char  *ins, unsigned int len);

  unwind_interval *new_ui(char *startaddr, ra_loc ra_status, 
			  unsigned int sp_ra_pos, int bp_ra_pos, 
			  bp_loc bp_status, int sp_bp_pos, int bp_bp_pos,
			  unwind_interval *prev);

  unwind_interval *fluke_ui(char *pc,unsigned int sp_ra_pos);

  void link_ui(unwind_interval *current, unwind_interval *next);
  void dump_ui(unwind_interval *u, int dump_to_stdout);

#ifdef __cplusplus
};
#endif



#endif
