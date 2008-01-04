extern unsigned long csprof_nm_addrs[];
extern int csprof_nm_addrs_len;

#define nm_table_last (csprof_nm_addrs_len - 1)

int 
nm_tab_bound(void **table, int length, void *pc, 
	     void **start, void **end)
{
  int lo, mid, high;
  int last = length - 1;

  if ((pc < table[0]) || (pc >= table[last])) {
    *start = 0;
    *end   = 0;
    return 1;
  }

  /*-------------------------------------------------------
   * binary search the table for the interval containing pc
   *-------------------------------------------------------*/
  lo = 0; high = last; mid = (lo + high) >> 1;
  for(; lo != mid;) {
    if (pc >= table[mid]) lo = mid;
    else high = mid;
    mid = (lo + high) >> 1;
  }

  *start = table[lo];
  *end   = table[lo+1];
  return 0;


#if 0
  for(i = 0; i < last; i++){
    if (table[i] == pc){
      *start = table[i];
      *end   = table[i+1];
      return 0;
    }
    if (table[i] > pc){
      *start = table[i-1];
      *end   = table[i];
      return 0;
    }
  }

  return 1;
#endif
}

#if STATIC_ONLY
int nm_bound(void *pc, void **start, void **end)
{
  return nm_tab_bound(csprof_nm_addrs, csprof_nm_addrs_len, pc, start, end);
}
#endif
