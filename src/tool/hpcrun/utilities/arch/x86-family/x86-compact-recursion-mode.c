#include <stdbool.h>
extern void hpcrun_real_collapse_recursion_mode(bool mode);

//
// x86 mode setting actually works
//
void
hpcrun_set_collapse_recursion_mode(bool mode)
{
  hpcrun_real_collapse_recursion_mode(mode);
}
