/*
 * Temporary stubs to make the static case link.
 * This is not the right way to fix the linking problem.
 */

#include <stdio.h>
#include "fnbounds_interface.h"

void
fnbounds_epoch_finalize()
{
    fprintf(stderr, "==> %s: should not be here\n", __func__);
}

