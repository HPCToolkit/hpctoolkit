extern unsigned long csprof_nm_addrs[];
extern int csprof_nm_addrs_len;

#define nm_table_last (csprof_nm_addrs_len - 1)

int nm_bound(unsigned long pc, unsigned long *start, unsigned long *end){
  int i;

  if ((pc < csprof_nm_addrs[0]) || (pc >= csprof_nm_addrs[nm_table_last])){
    *start = 0L;
    *end   = 0L;
    return 0;
  }
  for(i = 0; i < csprof_nm_addrs_len; i++){
    if (csprof_nm_addrs[i] == pc){
      *start = csprof_nm_addrs[i];
      *end   = csprof_nm_addrs[i+1];
      return 1;
    }
    if (csprof_nm_addrs[i] > pc){
      *start = csprof_nm_addrs[i-1];
      *end   = csprof_nm_addrs[i];
      return 1;
    }
  }
  return 0;
}
