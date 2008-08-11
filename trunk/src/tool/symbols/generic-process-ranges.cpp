/*
 * Generic (empty) definitions of the process range functions for
 * platforms that don't use this technique.
 *
 * $Id$
 */

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
