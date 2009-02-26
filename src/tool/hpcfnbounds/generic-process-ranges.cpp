// -*-Mode: C++;-*-
// $Id$

//***************************************************************************
// Generic (empty) definitions of the process range functions for
// platforms that don't use this technique.
//***************************************************************************

#include "process-ranges.h"

void
process_range_init(void)
{
  return;
}


void 
process_range(long offset, void *vstart, void *vend, bool fn_discovery)
{
  return;
}


bool
range_contains_control_flow(void *vstart, void *vend)
{
  return false;
}
