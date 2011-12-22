#include <stdbool.h>
extern void hpcrun_real_compact_recursion_mode(bool mode);

//
// ppc64 does NOT implement compact recursion mode (yet)
//
void
hpcrun_set_compact_recursion_mode(bool mode)
{
  hpcrun_real_compact_recursion_mode(false);
}
