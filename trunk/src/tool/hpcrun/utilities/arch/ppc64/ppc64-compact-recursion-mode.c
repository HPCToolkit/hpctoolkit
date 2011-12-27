#include <stdbool.h>
extern void hpcrun_real_collapse_recursion_mode(bool mode);

//
// ppc64 does NOT implement collapse recursion mode (yet)
//
void
hpcrun_set_collapse_recursion_mode(bool mode)
{
  hpcrun_real_collapse_recursion_mode(false);
}
