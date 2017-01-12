/*
 * uw_recipe.h
 *
 * Specification of uw_recipe_t, an abstract unwind recipe.
 * Known concrete implementations:
 *   x86recipe_t defined in x86-unwind-interval.h
 *   ppc64recipe_t defined in ppc64-unwind-interval.h
 *
 *      Author: dxnguyen
 */

#ifndef __UW_RECIPE_H__
#define __UW_RECIPE_H__

#include <lib/prof-lean/mem_manager.h>

//******************************************************************************
// macro
//******************************************************************************

#define MAX_RECIPE_STR 256


//******************************************************************************
// Abstract data type
//******************************************************************************

typedef struct recipe_s uw_recipe_t;

static inline uw_recipe_t*
uw_recipe_t_new(mem_alloc m_alloc, size_t recipe_size)
{
  return (uw_recipe_t*)m_alloc(recipe_size);
}

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
