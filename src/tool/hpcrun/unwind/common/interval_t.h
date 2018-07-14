#ifndef __PTRINTERVAL_T_H__
#define __PTRINTERVAL_T_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdint.h>

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/mem_manager.h>

//******************************************************************************
// macro
//******************************************************************************

#define MAX_INTERVAL_STR 64

//******************************************************************************
// type
//******************************************************************************

typedef struct interval_s {
  uintptr_t start;
  uintptr_t end;
} interval_t;


//******************************************************************************
// Comparison
//******************************************************************************

/*
 * pre-condition: *p1 and *p2 are interval_t and do not overlap.
 * if p1->start < p2->start returns -1
 * if p1->start == p2->start returns 0
 * if p1->start > p2->start returns 1
 */
int
interval_t_cmp(void* p1, void* p2);

/*
 * pre-condition:
 *   interval is a interval_t* of the form [start, end),
 *   address is uintptr_t
 * check if address is inside of interval, i.e. start <= address < end
 *
 *  // special boundary case:
 *  if (address == UINTPTR_MAX && interval.start == UINTPTR_MAX)
 *    return 0;
 *
 * if address < interval.start , i.e. interval is "greater than" address
 *   return 1
 * if interval.start <= address < interval.end, i.e. address is inside interval
 *   return 0
 * else , i.e. interval is "less than" address
 *   returns -1
 *
 */
int
interval_t_inrange(void* interval, void* address);

//******************************************************************************
// String output
//******************************************************************************

/*
 * concrete implementation of the abstract val_tostr function of the generic_val class.
 * pre-condition: ptr_interval is of type ptrinterval_t*
 * pre-condition: result[] is an array of length >= MAX_INTERVAL_STR
 * post-condition: result[] is a string of the form [interval.start, interval.end)
 */
void
interval_t_tostr(void* interval, char result[]);

void
interval_t_print(interval_t* interval);

/*
 * the max spaces occupied by "([start_address ... end_address)
 */
char*
interval_maxspaces();

#endif /* __PTRINTERVAL_T_H__ */
