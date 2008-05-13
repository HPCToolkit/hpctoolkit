#include "fnbounds_interface.h"

int 
fnbounds_table_lookup(void **table, int length, void *pc, 
		      void **start, void **end)
{
  int lo, mid, high;
  int last = length - 1;

  if ((pc < table[0]) || (pc >= table[last])) {
    *start = 0;
    *end   = 0;
    return 1;
  }

  //-------------------------------------------------------------------------------
  // binary search the table for the interval containing pc
  //-------------------------------------------------------------------------------
  lo = 0; high = last; mid = (lo + high) >> 1;
  for(; lo != mid;) {
    if (pc >= table[mid]) lo = mid;
    else high = mid;
    mid = (lo + high) >> 1;
  }

  *start = table[lo];
  *end   = table[lo+1];
  return 0;
}
