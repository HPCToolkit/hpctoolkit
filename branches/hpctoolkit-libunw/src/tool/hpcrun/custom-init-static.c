//
// run a special user initialization function, if the CUSTOM_INIT flag is set
//
// NOTE: the static case implemented in this file is a *NOOP*
//

// *********************************************
// system includes
// *********************************************

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <dlfcn.h>

// *********************************************
// local includes
// *********************************************

#include "custom-init.h"
#include <monitor.h>
#include <messages/messages.h>

// *********************************************
// interface functions
// *********************************************

void
hpcrun_do_custom_init(void)
{
  if (ENABLED(CUSTOM_INIT)) {
    TMSG(CUSTOM_INIT," -- not implemented for static case");
  }
}
