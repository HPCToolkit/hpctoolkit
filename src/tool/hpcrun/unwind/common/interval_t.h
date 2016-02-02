/*
 * interval_t.h
 *
 * Structure representing a right-opened interval of the form [a, b), where
 * a and b are pointers.
 *
 * Viewed as a concrete "subclass" of the generic_val class defined in
 * generic_val.h.
 *
 * Author: dxnguyen
 */

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
// Constructor
//******************************************************************************

// pre-condition: start <= end
interval_t*
interval_t_new(uintptr_t start, uintptr_t end, mem_alloc m_alloc);


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
 * pre-condition: interval is a interval_t*, address is uintptr_t
 * if address < interval->start return 1
 * if interval->start <= address < interval.end return 0
 * else returns -1
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
 * post-condition: result[] is a string of the form [interval->start, interval->end)
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
