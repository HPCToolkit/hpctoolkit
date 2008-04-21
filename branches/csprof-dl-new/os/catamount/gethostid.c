#ifdef CRAYXT_CATAMOUNT_TARGET
#include <catamount/data.h>

long int gethostid(void){
  return _my_pnid;
}

#else
#include "name.h"

long gethostid(void)
{
  return csprof_get_mpirank();
}
#endif
