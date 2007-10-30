#ifdef CRAYXT_CATAMOUNT_TARGET
#include <catamount/data.h>

long int gethostid(void){
  return _my_pnid;
}

#else
#include <unistd.h>
extern long gethostid(void);
#endif
