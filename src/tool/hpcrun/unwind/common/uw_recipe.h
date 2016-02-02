/*
 * uw_recipe.h
 *
 * Abstract unwind recipe
 *
 *      Author: dxnguyen
 */

#ifndef __UW_RECIPE_H__
#define __UW_RECIPE_H__

//******************************************************************************
// local include files
//******************************************************************************

#include <lib/prof-lean/mem_manager.h>

//******************************************************************************
// macro
//******************************************************************************

#define MAX_RECIPE_STR 256


//******************************************************************************
// Abstract data type
//******************************************************************************

typedef struct recipe_s uw_recipe_t;

/*
 * Concrete implementation of the abstract val_tostr function of the
 * generic_val class.
 * pre-condition: uwr is of type uw_recipe_t*
 */
void
uw_recipe_tostr(void* uwr, char str[]);

void
uw_recipe_print(void* uwr);


#endif /* __UW_RECIPE_H__ */
